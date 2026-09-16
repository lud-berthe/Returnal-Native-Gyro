#include "rg/steam_association.hpp"
#include "rg/steam_contacts.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
 using namespace rg;constexpr std::uint64_t h=557011432378749,other=h+1;
 std::array<SteamGamepadIdentity,1> waking{{{0,0,true}}};
 check(selectSteamGamepad(waking,h,0,h)==0,"observed startup: missing SDL handle uses confirmed Steam XInput slot");
 check(selectSteamGamepad(waking,h,0,0)==-1,"ordinary Xbox slot has no Steam owner");
 check(selectSteamGamepad(waking,h,0,other)==-1,"slot must belong to the selected Steam controller");
 check(selectSteamGamepad(waking,h,1,h)==-1,"both slot directions must agree");
 check(selectSteamGamepad(waking,0,0,0)==-1,"zero selected handle cannot attach");
 waking[0].xinput=false;check(selectSteamGamepad(waking,h,0,h)==-1,"a generic player index is not an XInput slot");waking[0].xinput=true;
 for(int slot:{-1,4,20}){waking[0].slot=slot;check(selectSteamGamepad(waking,h,slot,h)==-1,"only XInput slots zero through three qualify");}waking[0].slot=0;
 waking[0].handle=h;check(selectSteamGamepad(waking,h,0,h)==0,"late metadata keeps the same connection");
 waking[0].handle=other;check(selectSteamGamepad(waking,h,0,h)==-1,"nonzero contradictory identity overrides slot fallback");
 std::array<SteamGamepadIdentity,2> pads{{{0,0,true},{h,-1,false}}};
 check(selectSteamGamepad(pads,h,0,h)==1,"exact Steam handle has priority over fallback");
 pads[1]={0,0,true};check(selectSteamGamepad(pads,h,0,h)==-1,"duplicate slot candidates are ambiguous");
 pads[0]={h,0,true};pads[1]={h,1,true};check(selectSteamGamepad(pads,h,0,h)==-1,"duplicate exact identities are ambiguous");
 pads[0]={0,0,true};pads[1]={0,1,true};check(selectSteamGamepad(pads,h,1,h)==1,"controller order does not replace slot identity");
 check(selectSteamGamepad({},h,0,h)==-1,"disconnect invalidates association");
 check(steamGamepadMatches({0,1,true},h,1,h),"reconnect can use a new slot before metadata");
 check(!steamGamepadMatches({0,1,true},h,1,other),"running fallback drops a reassigned slot");
 check(steamContactAssociation(1,1)&&!steamContactAssociation(1,2)&&!steamContactAssociation(2,1),"raw sensors remain single-stream even with slot fallback");
 std::cout<<"Steam startup association: missing/late identity, reconnect, slot reassignment, ambiguity and XInput guards passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
