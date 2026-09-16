#include "rg/runtime.hpp"
namespace rg {
Runtime* runtime{};
Settings Runtime::snapshot(){std::lock_guard lock(settingsMutex);return settings;}
void Runtime::update(const Settings& value){
    // Apply the same validation to UI and file changes.
    auto valid=parseConfig(serializeConfig(value));
    {std::lock_guard lock(settingsMutex);settings=valid.settings;}
    mixedFlags.store((valid.settings.MixedInput?1u:0u)|(valid.settings.IgnoreMouseMotion?2u:0u)|(static_cast<unsigned>(valid.settings.HudMode)<<2));
    revision.fetch_add(1,std::memory_order_release);
}
void Runtime::log(const std::string& message){std::lock_guard lock(logMutex);logFile<<monotonicNs()/1'000'000<<" "<<message<<'\n'<<std::flush;}
}
