#include "rg/runtime.hpp"
#include "rg/mixed.hpp"
#include "rg/haptic_gate.hpp"
#include "rg/flick_stick.hpp"
#include "rg/aim_gate.hpp"
#include "rg/short_press.hpp"
#include <MinHook.h>
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>
#pragma comment(lib,"bcrypt.lib")
namespace rg {
namespace {
using Object=void*;
template<class T> T read(Object object,size_t offset){T value{};std::memcpy(&value,static_cast<const std::byte*>(object)+offset,sizeof(T));return value;}
template<class T> void write(Object object,size_t offset,T value){std::memcpy(static_cast<std::byte*>(object)+offset,&value,sizeof(T));}
template<class T> T symbol(HMODULE module,const char* name){auto value=GetProcAddress(module,name);if(!value)throw std::runtime_error(std::string("Missing export: ")+name);return reinterpret_cast<T>(value);}
HMODULE module(const wchar_t* name){return GetModuleHandleW(name);}
std::string digest(HMODULE handle){
    wchar_t path[32768]{};GetModuleFileNameW(handle,path,32768);
    std::ifstream file(std::filesystem::path(path),std::ios::binary);if(!file)return {};
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    struct Cleanup{BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}}cleanup{algorithm,hash};
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0)return {};
    std::array<unsigned char,65536> data{};
    while(file){file.read(reinterpret_cast<char*>(data.data()),data.size());if(BCryptHashData(hash,data.data(),static_cast<ULONG>(file.gcount()),0)<0)return {};}
    if(!file.eof())return {};
    std::array<unsigned char,32> bytes{};if(BCryptFinishHash(hash,bytes.data(),static_cast<ULONG>(bytes.size()),0)<0)return {};
    constexpr char hex[]="0123456789abcdef";std::string result;for(auto b:bytes){result+=hex[b>>4];result+=hex[b&15];}return result;
}
// Scan only executable PE sections once. Multiple matches are a failure.
void* unique(HMODULE handle,std::initializer_list<int> pattern){
    auto base=reinterpret_cast<std::byte*>(handle);auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    auto sections=IMAGE_FIRST_SECTION(nt);void* found=nullptr;
    for(unsigned s=0;s<nt->FileHeader.NumberOfSections;++s){
        if(!(sections[s].Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
        auto bytes=reinterpret_cast<unsigned char*>(base+sections[s].VirtualAddress);auto size=sections[s].Misc.VirtualSize;
        for(size_t i=0;i+pattern.size()<=size;++i){bool match=true;size_t j=0;for(int expected:pattern){if(expected>=0&&bytes[i+j]!=expected){match=false;break;}++j;}
            if(match){if(found)return nullptr;found=bytes+i;}
        }
    }return found;
}
struct Bindings {
    using Unary=bool(*)(Object);
    using Name=std::uint64_t;
    void(*makeName)(Name*,const wchar_t*,int){};
    Object(*findProperty)(Object,Name){};int(*offset)(Object){};
    bool(*boolValue)(Object,const void*){};bool(*childOf)(Object,Object){};
    Object(*findFunction)(Object,Name,int){};void(*processEvent)(Object,Object,void*){};
    Object(*getComponent)(Object,Object){};
    Unary local{},paused{},lookIgnored{},altActive{};
    bool(*cinematic)(Object,bool){};Object(*viewTarget)(Object){};
    bool(*gamepadKey)(const void*){};
    Object pcClass{},pawnClass{},saveClass{},altClass{},aimFunction{},focusClass{},interactionClass{},discoverableClass{};
    Object(*focusedInteractable)(Object){};Object(*currentDiscoverable)(Object){};bool(*interactionHoldRequired)(Object,Object){};float(*interactionDelay)(Object,Object){};
    bool(*secondaryInteractionAllowed)(Object){};bool(*secondaryIntercepting)(Object){};Object(*keysForAction)(Object,Name){};
    Name interactAction{},secondaryInteractAction{},discoverAction{};int playerInput{},riskyInteract{};
    Object(*getTriggerState)(Object,int){};int focusOwner{},focusPlayer{},altRequestOffset{};
    Object altRequestProperty{};Unary altInTrigger{};
    Object screenProperty{},controllerProperty{};
    int pawn{},follow{},save{},screen{},controller{},control{};
    const void *mouseX{},*mouseY{},*rightX{},*rightY{};
    std::array<const void*,12> buttons{};
    void*(*copyKey)(void*,const void*){};void(*destroyKey)(void*){};
    Name name(const wchar_t* text){Name result{};makeName(&result,text,0);return result;}
    Object field(Object cls,const wchar_t* text){auto property=findProperty(cls,name(text));if(!property)throw std::runtime_error("Required reflected property missing");return property;}
    int fieldOffset(Object cls,const wchar_t* text,int expected){auto value=offset(field(cls,text));if(value!=expected)throw std::runtime_error("Reflected layout disagrees with supported profile");return value;}
    bool is(Object obj,Object cls){return obj&&cls&&childOf(read<Object>(obj,0x10),cls);}
} b;
void initializeReflection(){
    auto game=module(L"Returnal-Returnal-Win64-Shipping.dll");
    b.pcClass=symbol<Object(*)()>(game,"?StaticClass@ATouristPlayerController@@SAPEAVUClass@@XZ")();
    b.pawnClass=symbol<Object(*)()>(game,"?GetPrivateStaticClass@APlayerCharacter@@CAPEAVUClass@@XZ")();
    b.saveClass=symbol<Object(*)()>(game,"?GetPrivateStaticClass@UTouristSaveGame@@CAPEAVUClass@@XZ")();
    b.altClass=symbol<Object(*)()>(game,"?StaticClass@UAltFireComponent@@SAPEAVUClass@@XZ")();
    b.focusClass=symbol<Object(*)()>(game,"?StaticClass@UFocusAimComponent@@SAPEAVUClass@@XZ")();
    b.altRequestOffset=b.fieldOffset(b.altClass,L"bWantsToAltFireMode",624);
    b.altRequestProperty=b.field(b.altClass,L"bWantsToAltFireMode");
    b.focusOwner=b.fieldOffset(b.focusClass,L"OwnerPlayerController",360);
    b.focusPlayer=b.fieldOffset(b.focusClass,L"OwnerPlayer",352);
    b.interactionClass=symbol<Object(*)()>(game,"?GetPrivateStaticClass@UInteractionManagerComponent@@CAPEAVUClass@@XZ")();
    b.discoverableClass=symbol<Object(*)()>(game,"?GetPrivateStaticClass@UDiscoverableComponent@@CAPEAVUClass@@XZ")();
    b.playerInput=b.fieldOffset(b.pcClass,L"PlayerInput",1064);
    b.interactAction=b.name(L"Interact");b.secondaryInteractAction=b.name(L"SecondaryInteract");b.discoverAction=b.name(L"Discover");
    b.pawn=b.fieldOffset(b.pcClass,L"Pawn",816);b.follow=b.fieldOffset(b.pawnClass,L"FollowCameraActor",3824);
    b.save=b.fieldOffset(b.pcClass,L"TouristSaveInstance",1792);b.control=b.fieldOffset(b.pcClass,L"ControlRotation",872);
    b.riskyInteract=b.fieldOffset(b.saveClass,L"RiskyInteract",414);
    b.screen=b.fieldOffset(b.pcClass,L"bIsInScreenspace",1914);b.screenProperty=b.field(b.pcClass,L"bIsInScreenspace");
    b.controller=b.fieldOffset(b.saveClass,L"bIsControllerUsed",2169);b.controllerProperty=b.field(b.saveClass,L"bIsControllerUsed");
    b.aimFunction=b.findFunction(b.pawnClass,b.name(L"IsAiming"),0);
    if(!b.aimFunction||b.fieldOffset(b.aimFunction,L"ReturnValue",0)!=0)throw std::runtime_error("IsAiming reflection validation failed");
}
std::atomic<bool> reflectionReady{};
using Rotation=void(*)(Object,float);Rotation originalRotation{};
using SetInput=void(*)(Object,Object,const void*);SetInput originalSet{};
using CommitInput=void(*)(Object,Object);CommitInput originalCommit{};
using MouseMove=bool(*)(Object,const void*,bool);MouseMove originalMouse{};
thread_local unsigned mouseDepth{};
bool mouseMove(Object slate,const void* event,bool synthetic){
    struct Scope{Scope(){++mouseDepth;}~Scope(){--mouseDepth;}}scope;
    return originalMouse(slate,event,synthetic);
}
void setInput(Object save,Object context,const void* key){
    auto& r=*runtime;++r.inputCalls;
    if(reflectionReady.load(std::memory_order_acquire)&&!r.suspended.load()&&b.is(save,b.saveClass)&&key){
        auto keyName=read<Bindings::Name>(const_cast<void*>(key),0);
        bool motion=keyName==read<Bindings::Name>(const_cast<void*>(b.mouseX),0)||keyName==read<Bindings::Name>(const_cast<void*>(b.mouseY),0)||(mouseDepth&&keyName==0);
        auto kind=motion?InputEventKind::MouseMotion:b.gamepadKey(key)?InputEventKind::Controller:InputEventKind::KeyboardOrMouseButton;
        unsigned flags=r.mixedFlags.load();
        if(suppressPresentation((flags&1)!=0,(flags&2)!=0,static_cast<int>(flags>>2),kind)){++r.suppressed;return;}
    }
    originalSet(save,context,key);
}
void commitInput(Object save,Object context){
    auto& r=*runtime;unsigned flags=r.mixedFlags.load();
    if(reflectionReady.load(std::memory_order_acquire)&&b.is(save,b.saveClass)){
        if(!r.suspended.load()&&(flags&1)&&(flags>>2)){
            // Unreflected pending bit: the unique SetInputType and commit signatures both validate 0xab3.
            write<unsigned char>(save,0xab3,(flags>>2)==1?1:0);
        }
        originalCommit(save,context);
        r.presentation=b.boolValue(b.controllerProperty,static_cast<std::byte*>(save)+b.controller)?1:0;
    }else originalCommit(save,context);
}
using LocalVirtual=void(*)(Object,Object,void*);LocalVirtual originalLocalVirtual{};
Object hapticClass{},hapticGraph{};unsigned char* mouseRumbleArguments{};std::atomic<DWORD> hapticThread{};
void prepareHapticGate(Object pc){
    auto cls=read<Object>(pc,0x10);if(cls==hapticClass)return;
    hapticThread.store(0,std::memory_order_release);
    hapticClass=cls;hapticGraph=nullptr;mouseRumbleArguments=nullptr;
    auto graph=b.findFunction(cls,b.name(L"ExecuteUbergraph_BP_PlayerController"),1);
    auto axis=b.findFunction(cls,b.name(L"InpAxisEvt_AxisMouse_K2Node_InputAxisEvent_2"),1);
    auto toggle=b.findFunction(cls,b.name(L"ToggleControllerVibration"),1);
    if(!graph||!axis||!toggle){runtime->log("Mouse vibration gate unavailable: Blueprint functions missing");return;}
    // UStruct::Script and FFrame::Code layout are guarded by the CoreUObject SHA profile.
    auto code=read<unsigned char*>(graph,0x60),axisCode=read<unsigned char*>(axis,0x60);
    int n=read<int>(graph,0x68),axisSize=read<int>(axis,0x68);
    auto condition=b.findProperty(toggle,b.name(L"Condition"));
    if(!code||!axisCode||n!=13232||axisSize!=36||!condition||b.offset(condition)!=0||!validMouseRumbleBranch({code,static_cast<size_t>(n)},{axisCode,static_cast<size_t>(axisSize)},reinterpret_cast<std::uintptr_t>(graph),read<Bindings::Name>(toggle,0x18))){runtime->log("Mouse vibration gate unavailable: Blueprint layout changed");return;}
    hapticGraph=graph;mouseRumbleArguments=code+mouseRumbleCall+1;
    hapticThread.store(GetCurrentThreadId(),std::memory_order_release);
    runtime->log("Mouse vibration gate validated: native rumble settings retained");
}
void localVirtual(Object context,Object frame,void* result){
    auto& r=*runtime;
    if(hapticThread.load(std::memory_order_acquire)==GetCurrentThreadId()&&mouseRumbleArguments&&read<Object>(frame,0x10)==hapticGraph&&read<unsigned char*>(frame,0x20)==mouseRumbleArguments&&!r.suspended.load()){
        unsigned flags=r.mixedFlags.load();
        if(suppressPresentation((flags&1)!=0,(flags&2)!=0,static_cast<int>(flags>>2),InputEventKind::MouseMotion)&&b.is(context,b.pcClass)&&b.local(context)){
            // Skip only ToggleControllerVibration(false) in the mouse-axis branch.
            // Its complete arguments are a serialized FName, EX_False and EX_EndFunctionParms.
            // No input, owned temporary, game vibration preference or other call is consumed.
            write(frame,0x20,mouseRumbleArguments+rumbleCallSize-1);
            static bool reported=false;if(!reported){reported=true;r.log("Mouse vibration-disable command suppressed");}
            return;
        }
    }
    originalLocalVirtual(context,frame,result);
}
GameplayState currentGameplay(Object pc){
    auto& r=*runtime;Object pawn=read<Object>(pc,b.pawn);
    bool validPawn=b.is(pawn,b.pawnClass),aiming=false,alt=false;
    unsigned blocked=validPawn?0u:1u;
    if(b.paused(pc))blocked|=2;
    if(b.lookIgnored(pc))blocked|=4;
    if(b.cinematic(pc,true))blocked|=8;
    if(b.boolValue(b.screenProperty,static_cast<std::byte*>(pc)+b.screen))blocked|=16;
    if(validPawn){auto view=b.viewTarget(pc);if(view!=pawn&&view!=read<Object>(pawn,b.follow))blocked|=32;}
    DWORD foregroundPid{};GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
    if(foregroundPid!=GetCurrentProcessId())blocked|=64;
    if(r.panelOpen.load())blocked|=128;
    if(r.suspended.load())blocked|=256;
    r.blockedReasons=blocked;bool allowed=blocked==0;
    if(validPawn){
        b.processEvent(pawn,b.aimFunction,&aiming);
        auto component=b.getComponent(pawn,b.altClass);if(component)alt=b.altActive(component);
    }
    return {allowed,aiming,alt,r.panelOpen.load()||r.suspended.load()};
}
bool actionUsesKey(Object playerInput,Bindings::Name action,Bindings::Name key){
    // GetKeysForAction returns the live per-player mappings, including remaps, without transferring ownership.
    auto list=b.keysForAction(playerInput,action);if(!list)return false;
    auto rows=read<unsigned char*>(list,0);int count=read<int>(list,8);
    if(!rows||count<0||count>128)return false;
    for(int i=0;i<count;++i){auto row=rows+static_cast<size_t>(i)*40;
        if(read<Bindings::Name>(row,0)==action&&read<Bindings::Name>(row,16)==key)return true;
    }
    return false;
}
bool nativeInteractionHold(Object pc,Bindings::Name key){
    auto input=read<Object>(pc,b.playerInput);if(!input)return false;
    bool primary=actionUsesKey(input,b.interactAction,key);
    bool secondary=actionUsesKey(input,b.secondaryInteractAction,key);
    bool discovery=actionUsesKey(input,b.discoverAction,key);
    if(!primary&&!secondary&&!discovery)return false;
    auto pawn=read<Object>(pc,b.pawn);if(!b.is(pawn,b.pawnClass))return false;
    auto manager=b.getComponent(pawn,b.interactionClass);if(!manager)return false;
    // Discovery has its own native action and target; GetFocusedInteractable is
    // deliberately empty while an undiscovered item is selected. Observe the
    // same target as LocalDoDiscover, without starting/completing discovery.
    if(discovery&&b.is(b.currentDiscoverable(manager),b.discoverableClass))return true;
    if(!primary&&!secondary)return false;
    auto focused=b.focusedInteractable(manager);if(!focused)return false;
    if(!primary&&(!b.secondaryIntercepting(manager)||!b.secondaryInteractionAllowed(focused)))return false;
    if(b.interactionHoldRequired(focused,pc))return true;
    // Optional delayed interactions also need their full press cycle in the
    // native HoldToInteract preference (EInteractType=0). Press/instant modes
    // do not require this extra exception. Query, never change, the preference.
    auto save=read<Object>(pc,b.save);
    if(!b.is(save,b.saveClass)||read<unsigned char>(save,b.riskyInteract)!=0)return false;
    const float delay=b.interactionDelay(focused,pc);
    return std::isfinite(delay)&&delay>0.f;
}
using KeyInput=bool(*)(Object,const void*,int,float,bool);
KeyInput originalKey{};
std::array<ShortPressGate,12> buttonGates;
Object buttonOwner{};std::uint32_t gatedBinding{};
bool inputKey(Object pc,const void* key,int event,float amount,bool gamepad){
 auto& r=*runtime;
 if(!key||!reflectionReady.load(std::memory_order_acquire)||!b.is(pc,b.pcClass)||!b.local(pc))return originalKey(pc,key,event,amount,gamepad);
 auto keyName=read<Bindings::Name>(const_cast<void*>(key),0);int id=0;
 for(int i:{1,2,3,4,7,10,11})if(keyName==read<Bindings::Name>(const_cast<void*>(b.buttons[i]),0)){id=i;break;}
 if(!id)return originalKey(pc,key,event,amount,gamepad);
 auto settings=r.snapshot();auto now=monotonicNs();
 if(buttonOwner!=pc){buttonGates={};buttonOwner=pc;}
 if(gatedBinding!=gyroGameButtonSelection(settings,r.availableButtons.load())){for(auto& gate:buttonGates)gate.cancel();gatedBinding=gyroGameButtonSelection(settings,r.availableButtons.load());}
 auto sampleAt=r.sampleTime.load();bool freshSensor=sampleAt&&now>=sampleAt&&now-sampleAt<200'000'000;
 bool eligible=gamepad&&freshSensor&&settings.GyroEnabled&&(gyroGameButtonSelection(settings,r.availableButtons.load())&buttonMask(id))!=0&&currentGameplay(pc).allowed;
 bool interactionHold=event==0&&eligible&&!buttonGates[id].pending()&&nativeInteractionHold(pc,keyName);
 auto result=buttonGates[id].event(event,now,eligible,interactionHold);
 if(interactionHold)r.log("Gyro button native interaction hold forwarded: id="+std::to_string(id));
 if(result==KeyDisposition::Forward)return originalKey(pc,key,event,amount,gamepad);
 if(result==KeyDisposition::Tap){
  // FKey is a 24-byte ref-counted indirect value. Each native call consumes one copy.
  alignas(8) std::array<std::byte,24> pressed{};b.copyKey(pressed.data(),key);
  originalKey(pc,pressed.data(),0,1.f,gamepad);
  r.log("Gyro button short tap forwarded: id="+std::to_string(id));
  return originalKey(pc,key,1,amount,gamepad);
 }
 if(event==0)r.log("Gyro button game action deferred: id="+std::to_string(id));
 if(event==1)r.log("Gyro button hold/cancel suppressed: id="+std::to_string(id));
 b.destroyKey(const_cast<void*>(key));return true;
}
using AimTriggerChanged=void(*)(Object,int);
AimTriggerChanged originalAimTrigger{};Object aimOwner{},aimPawn{};int aimIndex{-1};
void aimTriggerChanged(Object component,int index){
 originalAimTrigger(component,index);
 if(reflectionReady.load(std::memory_order_acquire)&&b.is(component,b.focusClass)){
  auto pc=read<Object>(component,b.focusOwner);
  if(b.is(pc,b.pcClass)&&b.local(pc)){aimOwner=pc;aimPawn=read<Object>(component,b.focusPlayer);aimIndex=index;}
 }
}
std::optional<unsigned> currentAimInput(Object pc,GameplayState animated){
 auto pawn=read<Object>(pc,b.pawn);
 if(!b.is(pawn,b.pawnClass))return {};
 std::optional<unsigned> trigger;
 if(aimOwner==pc&&aimPawn==pawn){
  if(auto state=b.getTriggerState(pawn,aimIndex))trigger=read<unsigned char>(state,14);
 }
 std::optional<bool> buttonRequest;
 if(auto component=b.getComponent(pawn,b.altClass);b.is(component,b.altClass)&&!b.altInTrigger(component)){
  // The game's separate AltFire action sets this input intent on press and clears
  // it on release (or the next press in native toggle mode), before animations.
  // Read the current pawn directly: no remembered callback/held state can go stale.
  buttonRequest=b.boolValue(b.altRequestProperty,static_cast<std::byte*>(component)+b.altRequestOffset);
 }
 auto combined=combinedAimCommands(trigger,buttonRequest);
 // Include source changes even if their combined eligibility remains identical.
 static int lastSources=-1;
 int sourceState=(trigger?static_cast<int>(*trigger):256)|(buttonRequest?(buttonRequest.value()?1024:512):0);
 if(sourceState!=lastSources){
  lastSources=sourceState;
  runtime->log("Aim input trigger="+(trigger?std::to_string(*trigger):"unavailable")+
    " separateAlt="+(buttonRequest?(buttonRequest.value()?std::string("held"):std::string("released")):std::string("unavailable"))+
    " combined="+(combined?std::to_string(*combined):"unavailable")+
    " animatedAim="+std::to_string(animated.aiming)+" animatedAlt="+std::to_string(animated.altFire));
 }
 return combined;
}
GameplayState flickGameplay(Object pc,GameplayState animated){
 if(auto flags=currentAimInput(pc,animated))return flickGameplayForTrigger(animated,*flags);
 return animated; // Preserve the established flick fallback before the first aim event.
}
using AxisInput=bool(*)(Object,const void*,float,float,int,bool);
AxisInput originalAxis{};
Object stickOwner{};float stickX{},stickY{};std::uint64_t stickAt{};
FlickStickProcessor flick;std::uint64_t lastFlickFrame{};
bool inputAxis(Object pc,const void* key,float value,float dt,int samples,bool gamepad){
 auto& r=*runtime;
 if(key&&reflectionReady.load(std::memory_order_acquire)&&b.is(pc,b.pcClass)&&b.local(pc)){
  auto keyName=read<Bindings::Name>(const_cast<void*>(key),0);
  bool horizontal=keyName==read<Bindings::Name>(const_cast<void*>(b.rightX),0);
  bool vertical=keyName==read<Bindings::Name>(const_cast<void*>(b.rightY),0);
  if(horizontal||vertical){
   if(stickOwner!=pc){stickOwner=pc;stickX=stickY=0;flick.reset();}
   if(horizontal)stickX=value;else stickY=flickVerticalFromGameAxis(value);stickAt=monotonicNs();r.flickX=stickX;r.flickY=stickY;
   auto settings=r.snapshot();
   if(flick.observeGameplay(settings.FlickStick,flickGameplay(pc,currentGameplay(pc)))){value=0;++r.flickSuppressed;}
  }
 }
 // FKey is passed indirectly by value on Win64. The original retains its sole destruction.
 return originalAxis(pc,key,value,dt,samples,gamepad);
}
void rotation(Object pc,float dt){
    auto& r=*runtime;
    static bool attempted=false;
    if(!attempted){
        attempted=true;
        try{initializeReflection();reflectionReady.store(true,std::memory_order_release);r.nativeReady=true;r.mixedReady=true;r.log("Game-thread reflection validated; native and mixed input ready");{std::lock_guard lock(r.diagnosticsMutex);r.status="Native gyro and mixed-input bindings ready";}}
        catch(const std::exception& e){r.suspended=true;r.log(std::string("Game-thread bindings unavailable: ")+e.what());}
    }
    if(!reflectionReady.load(std::memory_order_acquire)||!b.is(pc,b.pcClass)||!b.local(pc)){originalRotation(pc,dt);return;}
    prepareHapticGate(pc);
    ++r.cameraCalls;
    auto now=monotonicNs();auto game=currentGameplay(pc);auto aimInput=currentAimInput(pc,game);game.aimInputHeld=aimInput&&aimCommandHeld(*aimInput);
    bool allowed=game.allowed,aiming=game.aiming,alt=game.altFire;auto settings=r.snapshot();
    r.gameFlags.store((allowed?1u:0u)|(aiming?2u:0u)|(alt?4u:0u)|(game.aimInputHeld?8u:0u));r.gameTime.store(now,std::memory_order_release);
    bool consume=allowed&&settings.GyroEnabled&&gyroAimModeAllows(settings.ActivationMode,game.aimInputHeld)&&r.sensorActive.load(std::memory_order_acquire)&&(now-r.sampleTime.load(std::memory_order_acquire))<100'000'000;
    auto delta=r.queue.consume(now,consume,r.motionEpoch.load(std::memory_order_acquire));
    if(!allowed||!settings.GyroEnabled||gatedBinding!=gyroGameButtonSelection(settings,r.availableButtons.load())||now-r.sampleTime.load()>200'000'000)for(auto& gate:buttonGates)gate.cancel();
    if(lastFlickFrame&&now-lastFlickFrame>250'000'000)flick.reset();lastFlickFrame=now;
    // UE may stop sending unchanged zero axes. A released stick must finish its timed flick.
    bool freshStick=stickOwner==pc&&now>=stickAt&&(now-stickAt<200'000'000||std::hypot(stickX,stickY)<0.65f);
    double flickDelta=flick.processGameplay(stickX,stickY,dt,freshStick,flickGameplay(pc,game),settings.FlickSpinDurationMs,settings.FlickStick);
    delta.yawDegrees+=flickDelta;r.flickYaw=static_cast<float>(flickDelta);
    // RotationInput is the pre-camera FRotator in degrees, validated by UpdateRotation instructions.
    // Keep a double residual because the game's accumulator itself is float.
    static double residualYaw=0,residualPitch=0;
    if(!allowed){residualYaw=residualPitch=0;}
    auto add=[&](size_t offset,double input,double& residual){float before=read<float>(pc,offset);double requested=input+residual;float after=static_cast<float>(before+requested);write(pc,offset,after);residual=requested-(static_cast<double>(after)-before);};
    add(0x504,delta.yawDegrees,residualYaw);add(0x500,delta.pitchDegrees,residualPitch);
    r.appliedYaw=static_cast<float>(delta.yawDegrees);r.appliedPitch=static_cast<float>(delta.pitchDegrees);
    originalRotation(pc,dt);
    r.controlPitch=read<float>(pc,b.control);r.controlYaw=read<float>(pc,b.control+4);
}
bool hook(void* target,void* detour,void** original){
    if(!target)return false;auto created=MH_CreateHook(target,detour,original);
    if(created!=MH_OK){runtime->log(std::string("Hook creation failed: ")+MH_StatusToString(created));return false;}
    return true;
}
}
bool installBindings(Runtime& r){
    struct Profile{const wchar_t* module;const char* hash;};
    const Profile profiles[]={
        {L"Returnal-Returnal-Win64-Shipping.dll","269c8ef7c74b600cf9aeb542126c32064eed78b436dbfdd3c8d385ce1a5f8b45"},
        {L"Returnal-Engine-Win64-Shipping.dll","84ef78f2edb1545c2b9adcbe858e49e8e1ccec9512e9c676138cdb5755e4ec8d"},
        {L"Returnal-CoreUObject-Win64-Shipping.dll","a6268b266eeab5219cda60fa0fa2e7a531bb6e53f272f00634198778e02d62a0"},
        {L"Returnal-HMQGame-Win64-Shipping.dll","b860e96c107fab200b8ac1563cde3bc66d339c18567673004e2f56336625775d"},
        {L"Returnal-Slate-Win64-Shipping.dll","de6bd8145e1dd6f66db9ca16049cc5074251a7a829a60a08dc84b9101a290523"},
        {L"Returnal-InputCore-Win64-Shipping.dll","4547c64c1805a7f97cb359b2b437e2f9d2e1407e477748de00130e98a63443f9"},
        {L"Returnal-Core-Win64-Shipping.dll","ac7a32452dab0a2ba20690cc27b68a8a5772d7ddcff175130cb6f69ef18b91e6"},
        {L"Returnal-UMG-Win64-Shipping.dll","b9b7c4575fbf37cb9efafe1a41d201d2d2b2c0918f47216ad5d3efefe8ce7a46"}};
    for(int attempt=0;attempt<600;++attempt){bool all=true;for(auto& profile:profiles)if(!module(profile.module))all=false;if(all)break;Sleep(100);}
    for(auto& profile:profiles)if(!module(profile.module)||digest(module(profile.module))!=profile.hash){r.log("Unsupported module SHA-256; no game hooks installed");return false;}
    auto game=module(profiles[0].module),engine=module(profiles[1].module),objects=module(profiles[2].module),input=module(profiles[5].module),core=module(L"Returnal-Core-Win64-Shipping.dll");
    b.makeName=symbol<decltype(b.makeName)>(core,"??0FName@@QEAA@PEB_WW4EFindName@@@Z");
    b.findProperty=symbol<decltype(b.findProperty)>(objects,"?FindPropertyByName@UStruct@@QEBAPEAVFProperty@@VFName@@@Z");
    b.offset=symbol<decltype(b.offset)>(objects,"?GetOffset_ForInternal@FProperty@@QEBAHXZ");
    b.boolValue=symbol<decltype(b.boolValue)>(objects,"?GetPropertyValue@FBoolProperty@@QEBA_NPEBX@Z");
    b.childOf=symbol<decltype(b.childOf)>(objects,"?IsChildOf@UStruct@@QEBA_NPEBV1@@Z");
    b.findFunction=symbol<decltype(b.findFunction)>(objects,"?FindFunctionByName@UClass@@QEBAPEAVUFunction@@VFName@@W4Type@EIncludeSuperFlag@@@Z");
    b.processEvent=symbol<decltype(b.processEvent)>(objects,"?ProcessEvent@UObject@@UEAAXPEAVUFunction@@PEAX@Z");
    b.focusedInteractable=symbol<decltype(b.focusedInteractable)>(game,"?GetFocusedInteractable@UInteractionManagerComponent@@QEBAPEAVUObject@@XZ");
    b.currentDiscoverable=symbol<decltype(b.currentDiscoverable)>(game,"?GetDiscoverableComponent@UInteractionManagerComponent@@QEBAPEAVUDiscoverableComponent@@XZ");
    b.interactionHoldRequired=symbol<decltype(b.interactionHoldRequired)>(game,"?Execute_GetInteractionDelayRequired@IInteractable@@SA_NPEAVUObject@@PEAVATouristPlayerController@@@Z");
    b.interactionDelay=symbol<decltype(b.interactionDelay)>(game,"?Execute_GetInteractionDelay@IInteractable@@SAMPEAVUObject@@PEAVATouristPlayerController@@@Z");
    b.secondaryInteractionAllowed=symbol<decltype(b.secondaryInteractionAllowed)>(game,"?Execute_IsSecondaryInteractionAllowed@IInteractable@@SA_NPEBVUObject@@@Z");
    b.secondaryIntercepting=symbol<decltype(b.secondaryIntercepting)>(game,"?IsSecondaryInteractInterceptingInput@UInteractionManagerComponent@@QEAA_NXZ");
    b.keysForAction=symbol<decltype(b.keysForAction)>(engine,"?GetKeysForAction@UPlayerInput@@QEBAAEBV?$TArray@UFInputActionKeyMapping@@V?$TSizedDefaultAllocator@$0CA@@@@@VFName@@@Z");
    b.getComponent=symbol<decltype(b.getComponent)>(engine,"?GetComponentByClass@AActor@@QEBAPEAVUActorComponent@@V?$TSubclassOf@VUActorComponent@@@@@Z");
    b.local=symbol<Bindings::Unary>(engine,"?IsLocalPlayerController@AController@@QEBA_NXZ");
    b.paused=symbol<Bindings::Unary>(engine,"?IsPaused@APlayerController@@QEBA_NXZ");
    b.lookIgnored=symbol<Bindings::Unary>(engine,"?IsLookInputIgnored@AController@@UEBA_NXZ");
    b.altActive=symbol<Bindings::Unary>(game,"?IsAltFireModeActive@UAltFireComponent@@QEBA_NXZ");
    b.altInTrigger=symbol<Bindings::Unary>(game,"?IsAltFireInTrigger@UAltFireComponent@@QEBA_NXZ");
    b.cinematic=symbol<decltype(b.cinematic)>(game,"?IsInCinematicMode@ATouristPlayerController@@QEBA_N_N@Z");
    b.viewTarget=symbol<decltype(b.viewTarget)>(engine,"?GetViewTarget@APlayerController@@UEBAPEAVAActor@@XZ");
    b.gamepadKey=symbol<decltype(b.gamepadKey)>(input,"?IsGamepadKey@FKey@@QEBA_NXZ");
    b.mouseX=symbol<void*>(input,"?MouseX@EKeys@@2UFKey@@B");b.mouseY=symbol<void*>(input,"?MouseY@EKeys@@2UFKey@@B");
    b.rightX=symbol<void*>(input,"?Gamepad_RightX@EKeys@@2UFKey@@B");b.rightY=symbol<void*>(input,"?Gamepad_RightY@EKeys@@2UFKey@@B");
    b.copyKey=symbol<decltype(b.copyKey)>(input,"??0FKey@@QEAA@AEBU0@@Z");
    b.destroyKey=symbol<decltype(b.destroyKey)>(input,"??1FKey@@QEAA@XZ");
    for(auto [id,key]:std::array<std::pair<int,const char*>,7>{{{1,"Gamepad_LeftShoulder"},{2,"Gamepad_RightShoulder"},{3,"Gamepad_LeftThumbstick"},{4,"Gamepad_RightThumbstick"},{7,"Gamepad_FaceButton_Right"},{10,"Gamepad_FaceButton_Left"},{11,"Gamepad_FaceButton_Top"}}}){
     auto exportName=std::string("?")+key+"@EKeys@@2UFKey@@B";b.buttons[id]=symbol<void*>(input,exportName.c_str());
    }
    auto keyTarget=symbol<void*>(engine,"?InputKey@APlayerController@@UEAA_NUFKey@@W4EInputEvent@@M_N@Z");
    constexpr unsigned char keyPrefix[]={0x48,0x89,0x74,0x24,0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xf0,0,0,0};
    if(std::memcmp(keyTarget,keyPrefix,sizeof(keyPrefix)))throw std::runtime_error("Button input instructions changed");
    auto aimTarget=symbol<void*>(game,"?OnAimTriggerStateChanged@UFocusAimComponent@@IEAAXH@Z");
    // This native callback resolves the command's indexed FInputTriggerState.
    auto callSite=static_cast<unsigned char*>(aimTarget)+0x19;
    if(callSite[0]!=0xe8)throw std::runtime_error("Aim input getter call changed");
    std::int32_t relative{};std::memcpy(&relative,callSite+1,4);
    auto getter=callSite+5+relative;
    if(getter!=reinterpret_cast<unsigned char*>(game)+0x3cd580)throw std::runtime_error("Aim input getter target changed");
    constexpr unsigned char getterPrefix[]={0x83,0xfa,0xff,0x74,0x1b,0x85,0xd2,0x78,0x36};
    if(std::memcmp(getter,getterPrefix,sizeof(getterPrefix)))throw std::runtime_error("Aim input getter instructions changed");
    b.getTriggerState=reinterpret_cast<decltype(b.getTriggerState)>(getter);
    auto axisTarget=symbol<void*>(engine,"?InputAxis@APlayerController@@UEAA_NUFKey@@MMH_N@Z");
    constexpr unsigned char axisPrefix[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x70};
    if(std::memcmp(axisTarget,axisPrefix,sizeof(axisPrefix)))throw std::runtime_error("Stick axis instructions changed; no hooks installed");
    auto rotationTarget=symbol<void*>(engine,"?UpdateRotation@APlayerController@@UEAAXM@Z");
    constexpr unsigned char rotationPrefix[]={0x40,0x53,0x48,0x83,0xec,0x70,0x8b,0x81,0x08,0x05,0x00,0x00,0x48,0x8d,0x54,0x24,0x20,0xc5,0xfb,0x10,0x81,0x00,0x05,0x00,0x00};
    if(std::memcmp(rotationTarget,rotationPrefix,sizeof(rotationPrefix)))throw std::runtime_error("Camera instructions changed; no hooks installed");
    auto setTarget=unique(game,{0x40,0x53,0x48,0x83,0xec,0x20,0x80,0xb9,0xe1,0x09,0,0,0,0x48,0x8b,0xd9,0x74,0x0f,0x49,0x8b,0xc8,0xff,0x15,-1,-1,-1,-1,0x88,0x83,0xb3,0x0a,0,0,0x48,0x83,0xc4,0x20,0x5b,0xc3});
    auto commitTarget=unique(game,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x0f,0xb6,0x81,0xb3,0x0a,0,0,0x40,0x32,0xed,0x48,0x8b,0xfa,0x48,0x8b,0xd9,0x3a,0x81,0x79,0x08,0,0});
    auto localVirtualTarget=symbol<void*>(objects,"?execLocalVirtualFunction@UObject@@SAXPEAV1@AEAUFFrame@@QEAX@Z");
    auto mouseTarget=symbol<void*>(module(profiles[4].module),"?ProcessMouseMoveEvent@FSlateApplication@@QEAA_NAEBUFPointerEvent@@_N@Z");
    if(!setTarget||!commitTarget)throw std::runtime_error("Mixed input signatures not unique; no hooks installed");
    if(MH_Initialize()!=MH_OK)throw std::runtime_error("MinHook initialization failed");
    std::vector<void*> targets;
    auto create=[&](void* target,void* detour,void** original){if(!hook(target,detour,original))return false;targets.push_back(target);return true;};
    bool ok=create(localVirtualTarget,reinterpret_cast<void*>(&localVirtual),reinterpret_cast<void**>(&originalLocalVirtual))&&create(keyTarget,reinterpret_cast<void*>(&inputKey),reinterpret_cast<void**>(&originalKey))&&create(aimTarget,reinterpret_cast<void*>(&aimTriggerChanged),reinterpret_cast<void**>(&originalAimTrigger))&&create(axisTarget,reinterpret_cast<void*>(&inputAxis),reinterpret_cast<void**>(&originalAxis))&&create(rotationTarget,reinterpret_cast<void*>(&rotation),reinterpret_cast<void**>(&originalRotation))&&create(setTarget,reinterpret_cast<void*>(&setInput),reinterpret_cast<void**>(&originalSet))&&create(commitTarget,reinterpret_cast<void*>(&commitInput),reinterpret_cast<void**>(&originalCommit))&&create(mouseTarget,reinterpret_cast<void*>(&mouseMove),reinterpret_cast<void**>(&originalMouse));
    if(!ok){for(auto target:targets)MH_RemoveHook(target);r.log("Hooks rolled back before activation");return false;}
    for(auto target:targets)MH_QueueEnableHook(target);
    if(MH_ApplyQueued()!=MH_OK){r.suspended=true;for(auto target:targets)MH_DisableHook(target);r.log("Hook activation failed; functionality disabled");return false;}
    r.log("Profile SHA-256 and unique signatures validated; waiting for game-thread reflection");
    return true;
}
}
