// Report interpretation is based on SDL's zlib-licensed PS4/PS5 HIDAPI drivers.
// This backend never calls SDL_hid_write or sends output/feature reports.
#include "rg/backend.hpp"
#include <SDL3/SDL_hidapi.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <vector>
namespace rg {
std::uint64_t monotonicNs(){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
namespace {
int signed16(const unsigned char* b){return static_cast<std::int16_t>(static_cast<unsigned>(b[0])|(static_cast<unsigned>(b[1])<<8));}
std::uint32_t unsigned32(const unsigned char* b){return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
std::uint32_t crc32(std::uint32_t crc,const unsigned char* data,size_t size){for(size_t i=0;i<size;++i){crc^=data[i];for(int n=0;n<8;++n)crc=(crc>>1)^(0xedb88320u&static_cast<unsigned>(-static_cast<int>(crc&1)));}return crc;}
bool crcValid(const unsigned char* data,size_t size){unsigned char header=0xa1;auto crc=crc32(0xffffffff,&header,1);crc=crc32(crc,data,size-4)^0xffffffff;return crc==unsigned32(data+size-4);}
class SonyBackend final:public MotionBackend {
    SDL_hid_device* device_{};
    DeviceInfo info_;
    std::string error_;
    bool ps5_{},primed_{};
    std::uint32_t previousTick_{};
    std::uint64_t ticks_{};
    std::array<float,6> bias_{},scale_{};
    std::uint64_t lastReportNs_{};
    void close(){if(device_)SDL_hid_close(device_);device_=nullptr;}
    void factoryCalibration() {
        bias_.fill(0);scale_={1.0f/16,1.0f/16,1.0f/16,1.0f/8192,1.0f/8192,1.0f/8192};
        if(!ps5_)scale_[3]=scale_[4]=scale_[5]=1.0f/8192; // Sony accelerometers: +/-4g, 8192 units/g
        std::array<unsigned char,64> data{};data[0]=ps5_?0x05:(info_.bluetooth?0x05:0x02);
        int n=SDL_hid_get_feature_report(device_,data.data(),ps5_?41:(info_.bluetooth?41:37));
        if(n<35){error_="Factory calibration unavailable; nominal scale and manual bias calibration used";return;}
        std::array<float,6> bias{},scale{};
        float speed=static_cast<float>(signed16(data.data()+19)+signed16(data.data()+21));
        if(speed<=0)return;
        for(int i=0;i<3;++i) {
            bias[i]=static_cast<float>(signed16(data.data()+1+i*2));
            int plus=(!ps5_&&info_.bluetooth)?7+i*2:7+i*4;
            int minus=(!ps5_&&info_.bluetooth)?13+i*2:9+i*4;
            float range=ps5_?static_cast<float>(signed16(data.data()+plus)-signed16(data.data()+minus)):
                std::abs(signed16(data.data()+plus)-bias[i])+std::abs(signed16(data.data()+minus)-bias[i]);
            if(range<=0)return;
            scale[i]=speed/range;
            int hi=signed16(data.data()+23+i*4),lo=signed16(data.data()+25+i*4);
            if(hi<=lo)return;
            bias[i+3]=static_cast<float>(hi+lo)*0.5f;scale[i+3]=2.0f/static_cast<float>(hi-lo);
        }
        for(int i=0;i<6;++i){float nominal=i<3?1.0f/16:1.0f/8192;if(std::abs(bias[i])>1024||std::abs(scale[i]/nominal-1)>0.5f)return;}
        bias_=bias;scale_=scale;info_.factoryCalibration=true;
    }
public:
    SonyBackend(){SDL_hid_init();}
    ~SonyBackend() override {close();SDL_hid_exit();}
    bool connect(int index) override {
        close();error_.clear();info_={};primed_=false;ticks_=0;
        struct Candidate{std::string path;unsigned product;bool bluetooth;std::string name;};
        std::vector<Candidate> candidates;
        auto list=SDL_hid_enumerate(0x054c,0);
        for(auto d=list;d;d=d->next) {
            if(d->usage_page!=1||d->usage!=5)continue;
            if(d->product_id!=0x05c4&&d->product_id!=0x09cc&&d->product_id!=0x0ce6&&d->product_id!=0x0df2)continue;
            std::string name=d->product_id==0x0df2?"DualSense Edge":d->product_id==0x0ce6?"DualSense":"DualShock 4";
            candidates.push_back({d->path,d->product_id,d->bus_type==SDL_HID_API_BUS_BLUETOOTH,name});
        }
        SDL_hid_free_enumeration(list);
        std::sort(candidates.begin(),candidates.end(),[](auto& a,auto& b){return a.path<b.path;});
        if(index<0||static_cast<size_t>(index)>=candidates.size()){error_="No selected Sony motion controller connected";return false;}
        auto c=candidates[static_cast<size_t>(index)];device_=SDL_hid_open_path(c.path.c_str());
        if(!device_){error_="Cannot open Sony controller with shared HID access";return false;}
        info_={c.name,c.path,0x054c,c.product,c.bluetooth,false};ps5_=c.product==0x0ce6||c.product==0x0df2;
        info_.diagram=ps5_?ControllerDiagram::DualSense:ControllerDiagram::DualShock;info_.layout=ControllerLayout::Sony;info_.touchpad=true;info_.availableButtons=analogStickCapabilities|standardGyroButtons|buttonMask(5)|(c.product==0x0df2?rearGyroButtons:0);
        factoryCalibration();lastReportNs_=monotonicNs();return true;
    }
    std::optional<GyroSample> read() override {
        if(!device_)return {};
        if(monotonicNs()-lastReportNs_>2'000'000'000){error_="No valid extended Sony motion reports; reconnect or use USB";close();return {};}
        std::array<unsigned char,128> bytes{};
        int n=SDL_hid_read_timeout(device_,bytes.data(),bytes.size(),5);
        if(n<0){error_="Sony controller disconnected";close();return {};}
        if(n==0){if(monotonicNs()-lastReportNs_>2'000'000'000){error_="No Sony motion reports; reconnect controller (Bluetooth requires an already enabled extended-report mode)";close();}return {};}
        size_t offset=0;
        if(ps5_){if(bytes[0]==1&&n>=64)offset=1;else if(bytes[0]==0x31&&n==78&&crcValid(bytes.data(),78))offset=2;else return {};}
        else {if(bytes[0]==1&&n>=64)offset=1;else if(bytes[0]==0x11&&n==78&&crcValid(bytes.data(),78))offset=3;else return {};}
        lastReportNs_=monotonicNs();
        const auto* p=bytes.data()+offset;std::uint32_t tick=ps5_?unsigned32(p+27):static_cast<std::uint16_t>(signed16(p+9));
        if(!primed_){previousTick_=tick;primed_=true;return {};}
        std::uint32_t delta=ps5_?tick-previousTick_:static_cast<std::uint16_t>(tick-previousTick_);
        if(delta==0)return {};
        previousTick_=tick;
        const double periodNs=ps5_?1000.0/3:16000.0/3;
        if(delta*periodNs>100'000'000){ticks_+=delta;return {};}
        ticks_+=delta;GyroSample s;s.sensorNs=static_cast<std::uint64_t>(ticks_*periodNs);s.arrivalNs=lastReportNs_;
        const size_t gyroOffset=ps5_?15:12,accelOffset=ps5_?21:18;
        float g[3],a[3];for(size_t i=0;i<3;++i){g[i]=(signed16(p+gyroOffset+i*2)-bias_[i])*scale_[i];a[i]=(signed16(p+accelOffset+i*2)-bias_[i+3])*scale_[i+3];}
        s.degreesPerSecond={g[0],g[1],g[2]};s.accelG={a[0],a[1],a[2]};
        s.buttons=sonyButtons({p,static_cast<size_t>(n)-offset},ps5_,info_.product==0x0df2);
        s.leftStickMagnitude=std::min(1.0f,std::hypot((p[0]-127.5f)/127.5f,(p[1]-127.5f)/127.5f));
        s.rightStickMagnitude=std::min(1.0f,std::hypot((p[2]-127.5f)/127.5f,(p[3]-127.5f)/127.5f));
        return s;
    }
    bool connected()const override{return device_!=nullptr;}
    DeviceInfo info()const override{return info_;}
    std::string error()const override{return error_;}
};
}
std::unique_ptr<MotionBackend> makeSonyPassiveBackend(){return std::make_unique<SonyBackend>();}
}

