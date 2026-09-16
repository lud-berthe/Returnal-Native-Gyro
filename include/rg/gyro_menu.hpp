#pragma once
#include "settings.hpp"
#include "controller_buttons.hpp"
#include <span>
#include <vector>
#include <string>
#include <string_view>
namespace rg {
struct GyroOption { const char* key;double step;int count; };
std::span<const GyroOption> gyroOptions();
std::span<const GyroOption> nativeGyroOptions();
std::vector<const GyroOption*> nativeVisibleOptions(std::uint32_t available);
int nativeFocusIndex(std::string_view key,std::uint32_t available);
int bindingFamily(int id);
int optionFamily(const GyroOption&);
bool nativeOptionVisible(const GyroOption&,std::uint32_t);
std::vector<int> bindingFamilies(std::uint32_t available);
std::vector<int> bindingChoices(int family,std::uint32_t available);
int nativeOptionCount(const Settings&,const GyroOption&,std::uint32_t);
int nativeOptionIndex(const Settings&,const GyroOption&,std::uint32_t);
void setNativeOptionIndex(Settings&,const GyroOption&,int,std::uint32_t);
std::string nativeOptionValue(const Settings&,const GyroOption&,int,std::string_view,ControllerLayout,std::uint32_t);
int optionCount(const GyroOption&,std::uint32_t available=allGyroButtons);
int optionIndex(const Settings&,const GyroOption&,std::uint32_t available=allGyroButtons);
void setOptionIndex(Settings&,const GyroOption&,int,std::uint32_t available=allGyroButtons);
std::string optionValue(const GyroOption&,int,std::string_view language,ControllerLayout layout=ControllerLayout::Sony,bool touchpad=true,std::uint32_t available=allGyroButtons);
std::string buttonLabel(int id,std::string_view language="en",ControllerLayout layout=ControllerLayout::Sony,bool touchpad=true);
std::string localize(std::string_view key,std::string_view language);
void resetGyroOptions(Settings&);
}
