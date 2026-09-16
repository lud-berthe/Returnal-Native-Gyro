#pragma once
#include "motion.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
namespace rg {
// SteamInput001 uses signed-short ranges stored in floats.
struct SteamMotionData {
 float quatX{},quatY{},quatZ{},quatW{};
 float accelX{},accelY{},accelZ{};
 float pitch{},roll{},yaw{};
};
static_assert(sizeof(SteamMotionData)==40);
inline std::optional<GyroSample> steamMotionSample(const SteamMotionData& m,std::uint64_t now){
 for(float v:std::array{m.accelX,m.accelY,m.accelZ,m.pitch,m.roll,m.yaw})if(!std::isfinite(v)||std::abs(v)>32768.0f)return {};
 double gravity=std::hypot(m.accelX,m.accelY,m.accelZ)*2.0f/32768.0f;
 if(gravity<0.05||gravity>3.5)return {}; // Unavailable/warming sensors must not look active.
 GyroSample s;s.sensorNs=now;s.arrivalNs=now;
 s.degreesPerSecond={m.pitch*2000.0f/32768.0f,m.yaw*2000.0f/32768.0f,m.roll*2000.0f/32768.0f};
 s.accelG={m.accelX*2.0f/32768.0f,m.accelZ*2.0f/32768.0f,-m.accelY*2.0f/32768.0f};return s;
}
}
