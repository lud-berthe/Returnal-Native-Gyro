#include "rg/motion.hpp"
#include <iostream>
#include <thread>
#include <limits>
#include <random>
#include <stdexcept>
namespace {
int checks=0;
void require(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
void near(double a,double b,double tolerance,const char* message){require(std::abs(a-b)<tolerance,message);}
rg::CameraDelta run(rg::Settings s,rg::Vec3 velocity,std::vector<double> intervals) {
    rg::MotionProcessor p;rg::GyroSample sample;sample.accelG={0,1,0};sample.degreesPerSecond=velocity;
    p.process(sample,s,{true});sample.sensorNs=1'000'000;p.process(sample,s,{true});
    rg::CameraDelta sum;
    for(double dt:intervals){sample.sensorNs+=static_cast<std::uint64_t>(std::llround(dt*1e9));auto d=p.process(sample,s,{true});sum.yawDegrees+=d.yawDegrees;sum.pitchDegrees+=d.pitchDegrees;}
    return sum;
}
}
int main(){try {
    auto baseline=[] {rg::Settings value;value.ActivationMode=0;value.GyroSpace=1;value.SensitivityX=value.SensitivityY=1;return value;};
    rg::Settings s=baseline();
    std::vector<double> steps(1000,0.001);
    auto zero=run(s,{},steps);near(zero.yawDegrees,0,1e-12,"zero yaw");near(zero.pitchDegrees,0,1e-12,"zero pitch");
    auto ten=run(s,{10,-10,0},steps);near(ten.yawDegrees,10,1e-6,"10deg/s one second yaw");near(ten.pitchDegrees,10,1e-6,"10deg/s one second pitch");
    s.SensitivityX=2;auto twenty=run(s,{10,-10,0},steps);near(twenty.yawDegrees,20,1e-6,"natural sensitivity");near(twenty.pitchDegrees,10,1e-6,"X adjustment preserves Y");
    s.LinkXY=false;s.SensitivityY=0.5;s.InvertX=true;s.InvertY=true;auto inverse=run(s,{10,-10,0},steps);near(inverse.yawDegrees,-20,1e-6,"invert X");near(inverse.pitchDegrees,-5,1e-6,"invert Y independent scale");
    s=baseline();
    auto tiny=run(s,{0,-1e-6f,0},steps);require(tiny.yawDegrees>0,"no hidden cutoff");
    std::vector<double> variable;for(int i=0;i<100;++i){variable.push_back(0.003);variable.push_back(0.007);}near(run(s,{0,-10,0},variable).yawDegrees,10,1e-5,"variable sensor intervals");
    rg::MotionProcessor calibrated;calibrated.setBias({0,-2,0});rg::GyroSample v;v.degreesPerSecond={0,-2,0};calibrated.process(v,s,{true});for(int i=1;i<=100;++i){v.sensorNs=i*1'000'000ull;near(calibrated.process(v,s,{true}).yawDegrees,0,1e-9,"bias subtraction");}
    for(int level=1;level<=3;++level){s.Acceleration=level;const double maximum[]={1,1.5,2,3};
        near(run(s,{0,-5,0},steps).yawDegrees,5,1e-5,"acceleration lower boundary");
        near(run(s,{0,-75,0},steps).yawDegrees,75*maximum[level],1e-5,"acceleration upper boundary");
        near(run(s,{0,-40,0},steps).yawDegrees,40*(1+(maximum[level]-1)*.5),1e-5,"acceleration interpolation");}
    float pitch=0,yaw=0;GamepadMotion::CalculatePlayerSpaceGyro(pitch,yaw,3,10,0,0,-1,0);near(pitch,3,1e-5,"Player Space local pitch");near(yaw,10,1e-5,"Player Space flat yaw");
    GamepadMotion::CalculatePlayerSpaceGyro(pitch,yaw,3,0,10,0,0,1);near(std::abs(yaw),10,1e-5,"Player Space upright roll");
    s=baseline();s.TighteningDps=2;
    near(run(s,{0,-1,0},steps).yawDegrees,0.5,1e-5,"tightening continuous gain");require(run(s,{0,-0.001f,0},steps).yawDegrees>0,"tightening not deadzone");
    s.TighteningDps=0;s.Smoothing=0;
    near(run(s,{0,-1,0},steps).yawDegrees,1,1e-6,"smoothing disabled raw");
    double previous=1;
    for(int ms:{0,5,50,250,500}){s.Smoothing=ms;double response=run(s,{0,-.1f,0},std::vector<double>(10,.001)).yawDegrees;require(response>0&&response<previous,"larger time constant softens onset without a cutoff");previous=response;}
    // Closed-form step response at the same elapsed time, for different sample
    // schedules. Recover velocity from the emitted camera delta, not internals.
    for(auto intervals:{std::vector<double>(100,.001),std::vector<double>(10,.010),std::vector<double>{.003,.017,.025,.005,.030,.020}}){
      s=baseline();s.Smoothing=50;s.SensitivityX=6;s.SensitivityY=4;
      rg::MotionProcessor p;rg::GyroSample input;input.accelG={0,1,0};
      p.process(input,s,{true});input.sensorNs=1'000'000;p.process(input,s,{true});
      input.degreesPerSecond={20,-100,0};rg::CameraDelta last;
      for(double dt:intervals){input.sensorNs+=static_cast<std::uint64_t>(std::llround(dt*1e9));last=p.process(input,s,{true});}
      near(last.yawDegrees/(intervals.back()*6),100*(1-std::exp(-2.0)),1e-9,"fast yaw follows 50ms exponential at every sample rate");
      near(last.pitchDegrees/(intervals.back()*4),20*(1-std::exp(-2.0)),1e-9,"pitch uses same time constant before independent sensitivity");
      input.degreesPerSecond={};input.sensorNs+=50'000'000;last=p.process(input,s,{true});
      near(last.yawDegrees/(.05*6),100*(1-std::exp(-2.0))*std::exp(-1.0),1e-9,"stationary decay follows time constant");
      s.Smoothing=500;input.sensorNs+=50'000'000;last=p.process(input,s,{true});
      near(last.yawDegrees/(.05*6),100*(1-std::exp(-2.0))*std::exp(-1.1),1e-9,"changing tau preserves filter state");
      s.GyroEnabled=false;input.sensorNs+=1'000'000;near(p.process(input,s,{true}).yawDegrees,0,1e-12,"disable clears smoothing tail immediately");
      s.GyroEnabled=true;input.sensorNs+=1'000'000;p.process(input,s,{true});input.sensorNs+=1'000'000;
      near(p.process(input,s,{true}).yawDegrees,0,1e-12,"reactivation never replays old smoothing tail");
    }
    s=baseline();
    for(int level=0;level<=3;++level){
      auto cfg=rg::parseConfig("ConfigVersion=3\nSmoothing="+std::to_string(level)+"\nSensitivityX=6\n");
      const int expected[]={0,20,40,80};require(cfg.settings.Smoothing==expected[level]&&cfg.settings.ConfigVersion==4,"legacy presets migrate to milliseconds once");
      require(rg::parseConfig(rg::serializeConfig(cfg.settings)).settings.Smoothing==expected[level],"milliseconds survive config reload");
      near(cfg.settings.SensitivityX,6,1e-9,"smoothing migration preserves other settings");
    }
    require(rg::parseConfig("ConfigVersion=4\nSmoothing=500\n").settings.Smoothing==500,"maximum smoothing accepted");
    require(rg::parseConfig("ConfigVersion=4\nSmoothing=503\n").settings.Smoothing==0,"out of range smoothing rejected");
    require(rg::parseConfig("ConfigVersion=4\nSmoothing=13\n").settings.Smoothing==15,"manual duration quantized to 5ms");
    s.Smoothing=0;s.Acceleration=3;s.SensitivityX=0;s.SensitivityY=2;near(run(s,{75,0,0},steps).pitchDegrees,450,1e-5,"acceleration preserves Y even with X zero");s=baseline();
    rg::MotionProcessor ratchet;rg::GyroSample sample;sample.degreesPerSecond={0,-10,0};s.ActivationButton=1;
    for(int i=0;i<10;++i){sample.sensorNs=i*1'000'000ull;ratchet.process(sample,s,{true});}
    sample.buttons=1;sample.sensorNs+=1'000'000;near(ratchet.process(sample,s,{true}).yawDegrees,0,1e-12,"ratchet immediate stop");
    sample.buttons=0;sample.sensorNs+=1'000'000;near(ratchet.process(sample,s,{true}).yawDegrees,0,1e-12,"no reposition replay");sample.sensorNs+=1'000'000;require(ratchet.process(sample,s,{true}).yawDegrees>0,"ratchet resumes next sample");
    auto dup=ratchet.process(sample,s,{true});near(dup.yawDegrees,0,1e-12,"duplicate timestamps dropped");
    sample.sensorNs+=1'000'000'000;near(ratchet.process(sample,s,{true}).yawDegrees,0,1e-12,"packet gap not integrated");
    for(int fps:{30,60,144,360,2000}){
        rg::MotionQueue<> queue;rg::CameraDelta sum;
        for(int i=1;i<=1000;++i){auto now=static_cast<std::uint64_t>(i)*1'000'000;require(queue.push({{0.01,0.02},now}),"queue sample push");if(i*fps/1000!=(i-1)*fps/1000){auto d=queue.consume(now,true);sum.yawDegrees+=d.yawDegrees;sum.pitchDegrees+=d.pitchDegrees;}}
        auto d=queue.consume(1'000'000'000,true);sum.yawDegrees+=d.yawDegrees;sum.pitchDegrees+=d.pitchDegrees;
        near(sum.yawDegrees,10,1e-9,"render framerate independent yaw");near(sum.pitchDegrees,20,1e-9,"render framerate independent pitch");
    }
    rg::MotionQueue<4> queue;require(queue.push({{1,2},1}),"push");near(queue.consume(200'000'000,true).yawDegrees,0,1e-12,"stale motion discarded");require(queue.push({{1,2},2}),"push2");near(queue.consume(3,false).yawDegrees,0,1e-12,"inactive motion discarded");
    auto invalid=rg::parseConfig("SensitivityX=nan\nSensitivityY=999\nGyroSpace=1.5\nGyroEnabled=2\nAccelerationStartDps=90\nAccelerationEndDps=10\n");require(invalid.warnings.size()==5,"config reports bad values");near(invalid.settings.SensitivityX,2.5,1e-9,"nonfinite config default");require(invalid.settings.GyroSpace==0&&invalid.settings.GyroEnabled,"invalid enum bool default");require(invalid.settings.AccelerationEndDps==75,"invalid interval default");
    auto roundtrip=rg::parseConfig(rg::serializeConfig(s));require(roundtrip.warnings.empty(),"config roundtrip");
    rg::MotionProcessor manual;manual.beginCalibration();s.CalibrationSeconds=1;sample={};sample.degreesPerSecond={0.1f,-0.2f,0.3f};
    for(int i=0;i<1100;++i){sample.sensorNs=i*1'000'000ull;manual.process(sample,s,{true});}
    require(manual.diagnostics().calibration==rg::CalibrationState::Complete,"manual calibration complete");near(manual.diagnostics().bias.y,-0.2,1e-5,"manual mean bias");
    std::cout<<"PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
