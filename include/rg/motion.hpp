#pragma once
#include "settings.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100) // Unused parameter in upstream GamepadMotionHelpers code.
#endif
#include <GamepadMotion.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
namespace rg {
struct Vec3 { float x{},y{},z{}; };
struct CameraDelta { double yawDegrees{},pitchDegrees{}; };
struct GyroSample {
    std::uint64_t sensorNs{}, arrivalNs{};
    Vec3 degreesPerSecond{},accelG{0,1,0};
    std::uint32_t buttons{};
    float rightStickMagnitude{},leftStickMagnitude{};
};
struct GameplayState { bool allowed{},aiming{},altFire{},overlay{},aimInputHeld{},menuOpen{}; };
enum class CalibrationEvent { None, AutomaticCorrection, ManualComplete };
enum class CalibrationState { Idle, Collecting, Complete };
struct MotionDiagnostics {
    Vec3 raw{},calibrated{},bias{},gravity{};
    double sensorHz{};
    CameraDelta delta{};
    CalibrationEvent calibrationEvent{};
    CalibrationState calibration{};
    float calibrationProgress{};
    bool active{};
    std::uint64_t samples{},discarded{};
};
class MotionProcessor {
public:
    CameraDelta process(const GyroSample&,const Settings&,GameplayState);
    void resetDevice();
    void resumeDevice();
    void setExternalCalibration(bool enabled);
    void beginCalibration();
    void resetCalibration();
    void setBias(Vec3 bias);
    const MotionDiagnostics& diagnostics() const {return diagnostics_;}
    bool toggleEnabled() const {return toggled_;}
private:
    GamepadMotion motion_;
    MotionDiagnostics diagnostics_;
    std::uint64_t previousNs_{};
    bool externalCalibration_{},gravityInitialized_{};
    bool primed_{},previousButton_{},toggled_{true},wasActive_{};
    int previousActivationMode_{-1},previousGyroSpace_{-1};
    double calibrationTime_{};
    Vec3 calibrationMean_{},calibrationM2_{};
    unsigned calibrationCount_{};
    CameraDelta filteredVelocity_{};
    void clearSmoothing();
    CameraDelta smooth(CameraDelta velocity,double dt,const Settings&);
};
struct TimedDelta {CameraDelta delta; std::uint64_t arrivalNs;std::uint64_t epoch{};};
// One producer (sensor), one consumer (game thread). Never overwrite unread data.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // Intentional cache-line padding isolates SPSC indices.
#endif
template<size_t N=2048> class MotionQueue {
    std::array<TimedDelta,N> values_{};
    alignas(64) std::atomic<size_t> write_{};
    alignas(64) std::atomic<size_t> read_{};
public:
    bool push(TimedDelta value) {
        auto w=write_.load(std::memory_order_relaxed),next=(w+1)%N;
        if(next==read_.load(std::memory_order_acquire))return false;
        values_[w]=value;write_.store(next,std::memory_order_release);return true;
    }
    CameraDelta consume(std::uint64_t nowNs,bool allow,std::uint64_t epoch=0) {
        CameraDelta result;
        auto r=read_.load(std::memory_order_relaxed),end=write_.load(std::memory_order_acquire);
        while(r!=end) {
            const auto& v=values_[r];
            if(allow&&v.epoch==epoch&&nowNs>=v.arrivalNs&&nowNs-v.arrivalNs<100'000'000){result.yawDegrees+=v.delta.yawDegrees;result.pitchDegrees+=v.delta.pitchDegrees;}
            r=(r+1)%N;
        }
        read_.store(r,std::memory_order_release);return result;
    }
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}
