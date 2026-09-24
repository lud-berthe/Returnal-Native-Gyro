#include "rg/motion.hpp"
#include "rg/controller_buttons.hpp"
#include "rg/aim_gate.hpp"
#include "rg/calibration_policy.hpp"
#include <algorithm>
namespace rg {
static bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
void MotionProcessor::clearSmoothing(){filteredVelocity_={};}
void MotionProcessor::resetDevice(){gravityInitialized_=false;motion_.Reset();motion_.ResetContinuousCalibration();motion_.PauseContinuousCalibration();diagnostics_={};previousNs_=0;primed_=false;previousButton_=false;toggled_=true;wasActive_=false;previousActivationMode_=-1;previousGyroSpace_=-1;calibrationTime_=0;calibrationCount_=0;calibrationMean_={};calibrationM2_={};clearSmoothing();}
void MotionProcessor::resumeDevice(){
    // Preserve fusion, calibration ownership and toggle state, but never replay
    // missing motion or a held-button transition from the disconnected interval.
    previousNs_=0;primed_=false;wasActive_=false;diagnostics_.active=false;diagnostics_.delta={};clearSmoothing();
}
void MotionProcessor::setExternalCalibration(bool enabled){if(externalCalibration_!=enabled){externalCalibration_=enabled;resetDevice();}}
void MotionProcessor::beginCalibration(){if(externalCalibration_)return;diagnostics_.calibration=CalibrationState::Collecting;diagnostics_.calibrationProgress=0;calibrationTime_=0;calibrationCount_=0;calibrationMean_={};calibrationM2_={};clearSmoothing();}
void MotionProcessor::resetCalibration(){if(externalCalibration_)return;motion_.ResetContinuousCalibration();motion_.PauseContinuousCalibration();diagnostics_.bias={};diagnostics_.calibration=CalibrationState::Idle;clearSmoothing();}
void MotionProcessor::setBias(Vec3 bias){if(externalCalibration_)return;motion_.SetCalibrationOffset(bias.x,bias.y,bias.z,1);diagnostics_.bias=bias;}
CameraDelta MotionProcessor::smooth(CameraDelta v,double dt,const Settings& s) {
    const double tau=std::clamp(s.Smoothing,0,500)/1000.0;
    if(tau>0){
        const double alpha=-std::expm1(-dt/tau);
        filteredVelocity_.yawDegrees+=alpha*(v.yawDegrees-filteredVelocity_.yawDegrees);
        filteredVelocity_.pitchDegrees+=alpha*(v.pitchDegrees-filteredVelocity_.pitchDegrees);
    }else filteredVelocity_=v;
    return filteredVelocity_;
}

CameraDelta MotionProcessor::process(const GyroSample& sample,const Settings& configured,GameplayState game) {
    const Settings& s=configured;
    // Keep the filtered velocity when only tau changes. A space change alters
    // the axis basis, so values from the old space cannot be carried across.
    if(previousGyroSpace_!=s.GyroSpace){clearSmoothing();previousGyroSpace_=s.GyroSpace;}
    diagnostics_.calibrationEvent=CalibrationEvent::None;diagnostics_.raw=sample.degreesPerSecond;diagnostics_.delta={};diagnostics_.active=false;
    if(!finite(sample.degreesPerSecond)||!finite(sample.accelG)){++diagnostics_.discarded;return {};}
    if(!gravityInitialized_){
        gravityInitialized_=motion_.InitializeGravityFromAcceleration(sample.accelG.x,sample.accelG.y,sample.accelG.z);
        if(!gravityInitialized_){++diagnostics_.discarded;return {};}
    }
    if(!primed_){previousNs_=sample.sensorNs;primed_=true;previousButton_=gyroActivationHeld(sample.buttons,s,sample.leftStickMagnitude,sample.rightStickMagnitude);return {};}
    if(sample.sensorNs<=previousNs_){++diagnostics_.discarded;return {};}
    double dt=static_cast<double>(sample.sensorNs-previousNs_)*1e-9;previousNs_=sample.sensorNs;
    if(dt>0.1){clearSmoothing();wasActive_=false;++diagnostics_.discarded;return {};}
    ++diagnostics_.samples;diagnostics_.sensorHz=1.0/dt;
    auto calibrationMode=GamepadMotionHelpers::CalibrationMode::Manual;
    if(automaticCalibrationAllowed(s.AutomaticCalibration,game.menuOpen,externalCalibration_)){
        // Menu-only calibration requires stillness; the anytime mode retains the
        // previous hybrid behavior. Switching modes preserves bias and gravity.
        calibrationMode=GamepadMotionHelpers::CalibrationMode::Stillness;
        if(s.AutomaticCalibration==1)calibrationMode=calibrationMode|GamepadMotionHelpers::CalibrationMode::SensorFusion;
    }
    if(diagnostics_.calibration==CalibrationState::Collecting)calibrationMode=GamepadMotionHelpers::CalibrationMode::Manual;
    motion_.SetCalibrationMode(calibrationMode);
    motion_.ProcessMotion(sample.degreesPerSecond.x,sample.degreesPerSecond.y,sample.degreesPerSecond.z,sample.accelG.x,sample.accelG.y,sample.accelG.z,static_cast<float>(dt));
    motion_.GetGravity(diagnostics_.gravity.x,diagnostics_.gravity.y,diagnostics_.gravity.z);
    motion_.GetCalibratedGyro(diagnostics_.calibrated.x,diagnostics_.calibrated.y,diagnostics_.calibrated.z);
    const auto previousBias=diagnostics_.bias;
    motion_.GetCalibrationOffset(diagnostics_.bias.x,diagnostics_.bias.y,diagnostics_.bias.z);
    if(automaticCalibrationAllowed(s.AutomaticCalibration,game.menuOpen,externalCalibration_)&&diagnostics_.calibration!=CalibrationState::Collecting&&
       (previousBias.x!=diagnostics_.bias.x||previousBias.y!=diagnostics_.bias.y||previousBias.z!=diagnostics_.bias.z))
        diagnostics_.calibrationEvent=CalibrationEvent::AutomaticCorrection;
    if(diagnostics_.calibration==CalibrationState::Collecting) {
        const auto g=sample.degreesPerSecond;float len=std::sqrt(g.x*g.x+g.y*g.y+g.z*g.z);
        const auto a=sample.accelG;float accel=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
        if(len>3.0f||std::abs(accel-1.0f)>0.08f){beginCalibration();return {};}
        ++calibrationCount_;calibrationTime_+=dt;
        const float values[]={g.x,g.y,g.z};
        auto update=[&](float value,float& avg,float& sum){float delta=value-avg;avg+=delta/static_cast<float>(calibrationCount_);sum+=delta*(value-avg);};
        update(values[0],calibrationMean_.x,calibrationM2_.x);update(values[1],calibrationMean_.y,calibrationM2_.y);update(values[2],calibrationMean_.z,calibrationM2_.z);
        if(calibrationCount_>20&&(calibrationM2_.x+calibrationM2_.y+calibrationM2_.z)/calibrationCount_>0.05f){beginCalibration();return {};}
        diagnostics_.calibrationProgress=static_cast<float>(calibrationTime_/s.CalibrationSeconds);
        if(calibrationTime_>=s.CalibrationSeconds&&calibrationCount_>=100){setBias(calibrationMean_);diagnostics_.calibration=CalibrationState::Complete;diagnostics_.calibrationProgress=1;diagnostics_.calibrationEvent=CalibrationEvent::ManualComplete;}
        return {};
    }
    bool held=gyroActivationHeld(sample.buttons,s,sample.leftStickMagnitude,sample.rightStickMagnitude);
    if(previousActivationMode_!=s.ActivationMode){previousActivationMode_=s.ActivationMode;previousButton_=held;toggled_=true;clearSmoothing();}
    // Menu/pause button presses must not silently toggle gameplay gyro.
    if(held&&!previousButton_&&s.ActivationMode==5&&s.GyroEnabled&&game.allowed&&!game.overlay)toggled_=!toggled_;previousButton_=held;
    bool enabled=true;
    switch(s.ActivationMode){case 1:case 2:enabled=gyroAimModeAllows(s.ActivationMode,game.aimInputHeld);break;case 3:enabled=held;break;case 4:enabled=!held;break;case 5:enabled=toggled_;break;default:break;}
    if(s.ActivationMode<=2)enabled=enabled&&!held; // Common button ratchets in automatic modes.
    enabled=enabled&&s.GyroEnabled&&game.allowed&&!game.overlay&&(!s.DisableWhileRightStick||sample.rightStickMagnitude<=s.RightStickThreshold);
    if(!enabled){clearSmoothing();wasActive_=false;return {};}
    if(!wasActive_){clearSmoothing();wasActive_=true;return {};}// drop the interval straddling an activation edge
    float pitch=diagnostics_.calibrated.x,yaw=diagnostics_.calibrated.y;
    switch(s.GyroSpace){case 0:motion_.GetPlayerSpaceGyro(pitch,yaw);break;case 2:yaw=-diagnostics_.calibrated.z;break;case 3:motion_.GetWorldSpaceGyro(pitch,yaw);break;default:break;}
    // SDL/Sony right-handed Y-up -> Unreal positive yaw right and pitch up.
    CameraDelta velocity{-static_cast<double>(yaw),static_cast<double>(pitch)};
    double rawSpeed=std::hypot(velocity.yawDegrees,velocity.pitchDegrees);
    velocity=smooth(velocity,dt,s);
    double speed=std::hypot(velocity.yawDegrees,velocity.pitchDegrees);
    double gain=s.TighteningDps>0?std::min(speed/s.TighteningDps,1.0):1.0;
    if(s.CutoffDps>0&&speed<s.CutoffDps)gain=0;
    double sx=s.SensitivityX,sy=s.SensitivityY;
    if(s.Acceleration){constexpr double maximum[]={1,1.5,2,3};double t=std::clamp((rawSpeed-5.0)/70.0,0.0,1.0);double acceleration=1+(maximum[std::clamp(s.Acceleration,0,3)]-1)*t;sx*=acceleration;sy*=acceleration;}
    gain*=game.altFire?s.AltFireMultiplier:(game.aiming?s.AimMultiplier:1);
    diagnostics_.delta={velocity.yawDegrees*dt*sx*gain*(s.InvertX?-1:1),velocity.pitchDegrees*dt*sy*gain*(s.InvertY?-1:1)};
    diagnostics_.active=true;return diagnostics_.delta;
}
}

