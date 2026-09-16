#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <span>
namespace rg {
struct Settings {
#define RG_SETTING(type,name,value,lo,hi,doc) type name = value;
#include "settings_fields.inc"
#undef RG_SETTING
};
struct SettingInfo {
    const char* name;
    const char* description;
    double minimum, maximum, defaultValue;
    bool integral;
    double (*get)(const Settings&);
    void (*set)(Settings&,double);
};
std::span<const SettingInfo> settingsSchema();
struct ConfigResult { Settings settings; std::vector<std::string> warnings; };
ConfigResult parseConfig(std::string_view text);
std::string serializeConfig(const Settings&);
ConfigResult loadConfig(const std::filesystem::path&);
bool saveConfig(const std::filesystem::path&,const Settings&,std::string& error);
}
