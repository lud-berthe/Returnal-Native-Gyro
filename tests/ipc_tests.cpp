#include "rg/ipc.hpp"
#include <iostream>
int main(){
    rg::SettingsChannel server(GetCurrentProcessId(),true),client(GetCurrentProcessId(),false);
    if(!server.valid()||!client.valid())return 1;
    {auto state=server.lock();if(!state)return 2;*state=rg::SharedSettings{};state->magic=rg::settingsMagic;state->abi=rg::settingsAbi;rg::sharedText(state->initialConfig,rg::serializeConfig(rg::Settings{}));}
    {auto state=client.lock();if(!state||state->magic!=rg::settingsMagic)return 3;auto settings=rg::parseConfig(state->initialConfig).settings;settings.GyroEnabled=true;rg::sharedText(state->requestedConfig,rg::serializeConfig(settings));state->requestVersion=1;state->visible=true;state->command=1;}
    {auto state=server.lock();if(!state||!state->visible||state->command!=1||state->requestVersion!=1||!rg::parseConfig(state->requestedConfig).settings.GyroEnabled)return 4;state->visible=false;state->acceptedVersion=1;}
    {auto state=client.lock();if(!state||state->visible||state->acceptedVersion!=1)return 5;}
    char shortText[4]{};rg::sharedText(shortText,"abcdef");if(std::string(shortText)!="abc")return 6;
    std::cout<<"PASS: independent mapped views exchange validated settings, visibility and calibration commands\n";return 0;
}
