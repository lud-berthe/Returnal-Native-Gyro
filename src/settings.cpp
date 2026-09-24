#include "rg/settings.hpp"
#include <charconv>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <mutex>
#include <type_traits>
#include <unordered_set>
#ifdef _WIN32
#include <windows.h>
#endif
namespace rg {
std::span<const SettingInfo> settingsSchema() {
    static const SettingInfo schema[] = {
#define RG_SETTING(type,name,value,lo,hi,doc) {#name,doc,lo,hi,value,std::is_integral_v<type>,[](const Settings& s)->double{return s.name;},[](Settings& s,double v){s.name=static_cast<type>(v);}},
#include "rg/settings_fields.inc"
#undef RG_SETTING
    };
    return schema;
}
static std::string_view trim(std::string_view s) {
    auto first=s.find_first_not_of(" \t\r\n");
    if(first==s.npos) return {};
    return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
ConfigResult parseConfig(std::string_view text) {
    ConfigResult r;
    std::unordered_set<std::string> seen;
    if(text.starts_with("\xef\xbb\xbf"))text.remove_prefix(3);
    while(!text.empty()) {
        auto end=text.find('\n'); auto line=trim(text.substr(0,end));
        text=end==text.npos?std::string_view{}:text.substr(end+1);
        if(line.empty()||line[0]==';'||line[0]=='#'||line[0]=='[')continue;
        auto eq=line.find('='); if(eq==line.npos){r.warnings.emplace_back("Ignored malformed line");continue;}
        auto key=trim(line.substr(0,eq)), val=trim(line.substr(eq+1));
        val=trim(val.substr(0,val.find_first_of(";#")));
        const SettingInfo* field=nullptr;
        for(const auto& f:settingsSchema())if(key==f.name){field=&f;break;}
        if(!field){r.warnings.emplace_back("Unknown key: "+std::string(key));continue;}
        if(!seen.emplace(key).second)r.warnings.emplace_back("Duplicate key: "+std::string(key));
        double value{}; auto [ptr,ec]=std::from_chars(val.data(),val.data()+val.size(),value);
        if(ec!=std::errc{}||ptr!=val.data()+val.size()||!std::isfinite(value)||value<field->minimum||value>field->maximum||(field->integral&&std::floor(value)!=value)) {
            field->set(r.settings,field->defaultValue);
            r.warnings.emplace_back("Invalid "+std::string(key)+"; restored default");
        } else field->set(r.settings,value);
    }
    if(!seen.contains("ConfigVersion")||r.settings.ConfigVersion<4){
        constexpr int milliseconds[]={0,20,40,80};
        if(r.settings.Smoothing<=3)r.settings.Smoothing=milliseconds[r.settings.Smoothing];
        else {r.settings.Smoothing=0;r.warnings.emplace_back("Invalid legacy Smoothing preset; restored Off");}
    }else if(r.settings.Smoothing%5){
        r.settings.Smoothing=static_cast<int>(std::lround(r.settings.Smoothing/5.0))*5;
        r.warnings.emplace_back("Smoothing rounded to nearest 5 ms");
    }
    const int legacyActivationMode=r.settings.ActivationMode;
    if(r.settings.ActivationMode==4)r.settings.ActivationMode=0;
    if(!seen.contains("ConfigVersion")||r.settings.ConfigVersion<2){
        // Automatic modes used a separate ratchet in schema 1; held/toggle modes used activation.
        if(legacyActivationMode<=2&&seen.contains("RatchetButton"))r.settings.ActivationButton=r.settings.RatchetButton;
    }
    if(r.settings.ActivationButton==6||r.settings.ActivationButton==8||r.settings.ActivationButton==9){r.settings.ActivationButton=0;r.warnings.emplace_back("Removed gyro button binding reset to None");}
    if(!seen.contains("ConfigVersion")||r.settings.ConfigVersion<3){
        int id=r.settings.ActivationButton;
        if(id==5||id==12||id==13||id==24)r.settings.GyroTouchpad=id;
        else if((id>=14&&id<=16)||id==25)r.settings.GyroStickSensor=id;
        else if((id>=17&&id<=19)||id==26)r.settings.GyroGripSensor=id;
        else if(id==27||id==28)r.settings.GyroStick=id;
        else r.settings.GyroButton=id;
    }
    auto validateFamily=[&](int& value,std::initializer_list<int> allowed){if(std::find(allowed.begin(),allowed.end(),value)==allowed.end()){value=0;r.warnings.emplace_back("Invalid gyro family binding reset to Off");}};
    validateFamily(r.settings.GyroButton,{0,1,2,3,4,7,10,11,20,21,22,23});
    validateFamily(r.settings.GyroTouchpad,{0,5,12,13,24});
    validateFamily(r.settings.GyroStickSensor,{0,14,15,16,25});
    validateFamily(r.settings.GyroGripSensor,{0,17,18,19,26});
    validateFamily(r.settings.GyroStick,{0,3,4,27,28});
    r.settings.ActivationButton=0;r.settings.RatchetButton=0;r.settings.LinkXY=false;r.settings.ConfigVersion=4;r.settings.DisableWhileRightStick=false;
    if(r.settings.AccelerationEndDps<=r.settings.AccelerationStartDps){
        r.settings.AccelerationStartDps=0;r.settings.AccelerationEndDps=75;
        r.warnings.emplace_back("Invalid acceleration interval; restored 0..75 deg/s");
    }
    return r;
}
std::string serializeConfig(const Settings& s) {
    std::ostringstream o;o.precision(8);
    o<<"; Returnal Native Gyro - values apply live unless marked restart required.\n; Booleans: 0 Off, 1 On. Manual calibration: use the controller prompt in Gyro Configuration.\n[ReturnalGyro]\n";
    for(const auto& f:settingsSchema())o<<"\n; "<<f.description<<'\n'<<f.name<<" = "<<f.get(s)<<'\n';
    return o.str();
}
ConfigResult loadConfig(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary);if(!in)return {Settings{}, {"Config unavailable; defaults selected"}};
    std::string text(65537,0);in.read(text.data(),static_cast<std::streamsize>(text.size()));text.resize(static_cast<size_t>(in.gcount()));
    if(text.size()>65536)return {Settings{}, {"Config exceeds 64 KiB; defaults selected"}};
    return parseConfig(text);
}
bool saveConfig(const std::filesystem::path& p,const Settings& s,std::string& error) {
    static std::mutex saveMutex;std::lock_guard lock(saveMutex);
    auto temporary=p;temporary+=".tmp";
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
    out<<serializeConfig(s);out.close();
    if(!out){error="Cannot write temporary configuration";return false;}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Cannot replace configuration: "+std::to_string(GetLastError());return false;}
#else
    std::error_code ec;std::filesystem::rename(temporary,p,ec);if(ec){error=ec.message();return false;}
#endif
    return true;
}
}
