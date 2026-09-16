#pragma once
#include "motion.hpp"
#include "controller_buttons.hpp"
#include <memory>
#include <optional>
#include <string>
namespace rg {
struct DeviceInfo {std::string name,path; unsigned vendor{},product{};bool bluetooth{},factoryCalibration{};ControllerLayout layout{ControllerLayout::Generic};bool touchpad{};bool connectionKnown{true};bool externalCalibration{};std::uint32_t availableButtons{standardGyroButtons};};
std::uint64_t monotonicNs();
class MotionBackend {
public:
    virtual ~MotionBackend()=default;
    virtual bool connect(int index)=0;
    virtual std::optional<GyroSample> read()=0;
    virtual bool connected() const=0;
    virtual DeviceInfo info() const=0;
    virtual std::string error() const=0;
};
std::unique_ptr<MotionBackend> makeSonyPassiveBackend();
std::unique_ptr<MotionBackend> makeSdlBackend();
std::unique_ptr<MotionBackend> makeSteamBackend();
std::unique_ptr<MotionBackend> makeMotionBackend(int mode);
}
