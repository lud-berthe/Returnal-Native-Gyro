#pragma once
#include <cstdint>
#include <optional>
namespace rg {
// Verified virtual gamepad slots only: a motion handle alone also exists when
// Steam Input is disabled. No SDL device scan is performed by this query.
std::optional<std::uint32_t> steamVirtualHidMask();
constexpr bool steamHasVirtualSlot(std::uint64_t handle,int slot,std::uint64_t reverseOwner){
    return handle&&slot>=0&&slot<4&&reverseOwner==handle;
}
// Wait for two observations separated by 250ms before accepting shared config.
// Steam may update it while the user changes the per-game controller setting.
class SteamHidFilterPolicy {
    std::optional<std::uint32_t> pending_, accepted_;
    std::uint64_t pendingAt_{};
public:
    struct Decision {bool refresh{}, rediscover{}; std::uint32_t value{};bool ready{};};
    Decision observe(std::uint32_t cached,std::uint32_t shared,std::uint64_t now,std::uint32_t steamOwned=0){
        if((cached|shared|steamOwned)&0xffff0000u){pending_.reset();return {};}
        // Shared flags describe global preferences, not the per-game override.
        // The inspected overlay uses its full mask for per-game Steam Input.
        // A reduced family mask also exposes Steam's virtual HID device and
        // makes SDL alternate between that device and XInput. Preserve Steam's
        // full enabled policy instead of reconstructing a partial family mask.
        const auto desired=steamOwned?0xffffu:shared;
        if(!pending_||*pending_!=desired){pending_=desired;pendingAt_=now;return {};}
        if(now<pendingAt_||now-pendingAt_<250'000'000)return {};
        bool changed=accepted_&&*accepted_!=desired;
        if(cached==desired)accepted_=desired;
        return {cached!=desired,changed||cached!=desired,desired,true};
    }
    void committed(std::uint32_t value){accepted_=value;}
};
struct SteamHidHotplugResult {
    bool rediscover{}, refreshed{}, unsupported{};
    std::uint32_t before{}, after{};
};
class SteamHidHotplug {
    void* module_{};
    std::uint64_t nextCheck_{};
    bool rejected_{};
    SteamHidFilterPolicy policy_;
public:
    SteamHidHotplug()=default;
    SteamHidHotplug(const SteamHidHotplug&)=delete;
    SteamHidHotplug& operator=(const SteamHidHotplug&)=delete;
    ~SteamHidHotplug();
    SteamHidHotplugResult update(std::uint64_t now);
};
}
