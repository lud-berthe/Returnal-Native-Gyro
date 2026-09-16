#include "rg/runtime.hpp"
#include "rg/gyro_menu.hpp"
#include "rg/short_press.hpp"
#include <array>
#include <MinHook.h>
#include <vector>
#include <cstring>
#include <stdexcept>
namespace rg { namespace {
using Obj=void*;using Name=std::uint64_t;
struct Str{wchar_t* data{};int count{},capacity{};};struct Text{alignas(8) std::byte bytes[24]{};};
struct Api{
 Obj objects{};void(*makeName)(Name*,const wchar_t*,int){};Obj(*findObject)(Obj,Obj,const wchar_t*,bool){};
 Obj(*property)(Obj,Name){};int(*offset)(Obj){};Obj(*function)(Obj,Name,int){};void(*event)(Obj,Obj,void*){};
 Obj(*createWidget)(Obj,Obj,Name){};Obj(*construct)(Obj,Obj,Name,unsigned,unsigned,Obj,bool,Obj,bool){};
 void(*setFocus)(Obj){};void(*copyValue)(Obj,void*,const void*){};bool(*focused)(Obj){};bool(*focusedChildren)(Obj){};void(*scrollIntoView)(Obj,Obj,bool,int,float){};void(*syncScroll)(Obj){};void(*syncSwitcherSlot)(Obj){};
 bool(*removeChild)(Obj,Obj){};Obj(*parent)(Obj){};bool(*visible)(Obj){};bool(*inViewport)(Obj){};void(*visibility)(Obj,unsigned char){};
 const void* triangle{};
 Obj(*scrollClass)(){};Obj(*addChild)(Obj,Obj){};Obj(*setContent)(Obj,Obj){};int(*count)(Obj){};Obj(*child)(Obj,int){};
 Text*(*textFrom)(Text*,const Str&){};Text*(*textAssign)(Text*,const Text&){};void(*textDestroy)(Text*){};
 Str*(*arrayType)(Obj,Str*,Str*,unsigned){};void(*arrayClear)(void*,Obj){};int(*arrayAdd)(void*,Obj,const void*){};Str*(*language)(Str*){};void(*freeMemory)(void*){};
} a;
Obj calibrationPrompt{};CalibrationCountdown calibrationCountdown;bool calibrationWasDown{},calibrationWaiting{},calibrationCollecting{},calibrationPromptActive{};ULONGLONG calibrationRequestedAt{},calibrationCompletedAt{};std::string calibrationLabel;
bool initialized{},failed{};ULONGLONG nextScan{};Obj pendingController{};int pendingIndex{},pendingSerial{};
struct Row{Obj widget{},spinner{};const GyroOption* option{};int index{};};
Obj controller{},root{},button{},page{};int controllerIndex{},controllerSerial{};std::vector<Row> rows;std::string language="en";Obj lastFocused{};ControllerLayout labelsLayout{ControllerLayout::Sony};bool labelsTouchpad{true},labelsExternalCalibration{};std::uint32_t labelsButtons{};
template<class T>T read(Obj object,size_t offset){T result{};std::memcpy(&result,static_cast<char*>(object)+offset,sizeof(T));return result;}
template<class T>void write(Obj object,size_t offset,T value){std::memcpy(static_cast<char*>(object)+offset,&value,sizeof(T));}
template<class T>T symbol(HMODULE module,const char* name){auto ptr=GetProcAddress(module,name);if(!ptr)throw std::runtime_error(std::string("Native menu missing export: ")+name);return reinterpret_cast<T>(ptr);}
Name name(const wchar_t* text){Name n{};a.makeName(&n,text,0);return n;}
Name newName(const wchar_t* text){Name n{};a.makeName(&n,text,1);return n;}
Obj prop(Obj object,const wchar_t* text){auto p=a.property(read<Obj>(object,0x10),name(text));if(!p)throw std::runtime_error("Native menu reflected property missing");return p;}
int offset(Obj object,const wchar_t* text,int expected=-1){int value=a.offset(prop(object,text));if(value<0||value>16384||(expected>=0&&value!=expected))throw std::runtime_error("Native menu property layout mismatch");return value;}
Obj field(Obj object,const wchar_t* text,int expected=-1){return read<Obj>(object,offset(object,text,expected));}
Obj findClass(const wchar_t* path){return a.findObject(nullptr,nullptr,path,false);}
void call(Obj object,const wchar_t* method,const void* parameters=nullptr,size_t size=0){auto f=a.function(read<Obj>(object,0x10),name(method),1);if(!f)throw std::runtime_error("Native menu Blueprint method missing: "+std::string(method,method+wcslen(method)));alignas(16) std::array<std::byte,4096> buffer{};if(size>buffer.size())throw std::runtime_error("Native menu parameter bound");if(parameters)std::memcpy(buffer.data(),parameters,size);a.event(object,f,buffer.data());}
std::wstring wide(const std::string& value){int size=MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0);std::wstring result(size,L' ');MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),size);return result;}
struct ScopedText {Text value;ScopedText(const std::string& text){auto w=wide(text);Str str{w.data(),static_cast<int>(w.size()+1),static_cast<int>(w.size()+1)};a.textFrom(&value,str);}~ScopedText(){a.textDestroy(&value);}};
void setText(Obj object,const wchar_t* property,const std::string& value){ScopedText text(value);a.textAssign(reinterpret_cast<Text*>(static_cast<char*>(object)+offset(object,property)),text.value);}
void init(){auto core=GetModuleHandleW(L"Returnal-Core-Win64-Shipping.dll"),objects=GetModuleHandleW(L"Returnal-CoreUObject-Win64-Shipping.dll"),umg=GetModuleHandleW(L"Returnal-UMG-Win64-Shipping.dll"),engine=GetModuleHandleW(L"Returnal-Engine-Win64-Shipping.dll");
 a.objects=symbol<Obj>(objects,"?GUObjectArray@@3VFUObjectArray@@A");a.makeName=symbol<decltype(a.makeName)>(core,"??0FName@@QEAA@PEB_WW4EFindName@@@Z");a.findObject=symbol<decltype(a.findObject)>(objects,"?StaticFindObject@@YAPEAVUObject@@PEAVUClass@@PEAV1@PEB_W_N@Z");
 a.property=symbol<decltype(a.property)>(objects,"?FindPropertyByName@UStruct@@QEBAPEAVFProperty@@VFName@@@Z");a.offset=symbol<decltype(a.offset)>(objects,"?GetOffset_ForInternal@FProperty@@QEBAHXZ");a.function=symbol<decltype(a.function)>(objects,"?FindFunctionByName@UClass@@QEBAPEAVUFunction@@VFName@@W4Type@EIncludeSuperFlag@@@Z");a.event=symbol<decltype(a.event)>(objects,"?ProcessEvent@UObject@@UEAAXPEAVUFunction@@PEAX@Z");
 a.createWidget=symbol<decltype(a.createWidget)>(umg,"?CreateWidgetInstance@UUserWidget@@SAPEAV1@AEAVUWidget@@V?$TSubclassOf@VUUserWidget@@@@VFName@@@Z");a.construct=symbol<decltype(a.construct)>(objects,"?StaticConstructObject_Internal@@YAPEAVUObject@@PEBVUClass@@PEAV1@VFName@@W4EObjectFlags@@W4EInternalObjectFlags@@1_NPEAUFObjectInstancingGraph@@5@Z");a.scrollClass=symbol<decltype(a.scrollClass)>(umg,"?GetPrivateStaticClass@UScrollBox@@CAPEAVUClass@@XZ");
 a.parent=symbol<decltype(a.parent)>(umg,"?GetParent@UWidget@@QEBAPEAVUPanelWidget@@XZ");
 a.visible=symbol<decltype(a.visible)>(umg,"?IsVisible@UWidget@@QEBA_NXZ");
 a.inViewport=symbol<decltype(a.inViewport)>(umg,"?IsInViewport@UUserWidget@@QEBA_NXZ");
 a.visibility=symbol<decltype(a.visibility)>(umg,"?SetVisibility@UWidget@@UEAAXW4ESlateVisibility@@@Z");
 a.triangle=symbol<void*>(GetModuleHandleW(L"Returnal-InputCore-Win64-Shipping.dll"),"?Gamepad_FaceButton_Top@EKeys@@2UFKey@@B");
 a.removeChild=symbol<decltype(a.removeChild)>(umg,"?RemoveChild@UPanelWidget@@QEAA_NPEAVUWidget@@@Z");
 a.addChild=symbol<decltype(a.addChild)>(umg,"?AddChild@UPanelWidget@@QEAAPEAVUPanelSlot@@PEAVUWidget@@@Z");a.setContent=symbol<decltype(a.setContent)>(umg,"?SetContent@UContentWidget@@QEAAPEAVUPanelSlot@@PEAVUWidget@@@Z");a.count=symbol<decltype(a.count)>(umg,"?GetChildrenCount@UPanelWidget@@QEBAHXZ");a.child=symbol<decltype(a.child)>(umg,"?GetChildAt@UPanelWidget@@QEBAPEAVUWidget@@H@Z");
 a.textFrom=symbol<decltype(a.textFrom)>(core,"?FromString@FText@@SA?AV1@AEBVFString@@@Z");a.textAssign=symbol<decltype(a.textAssign)>(core,"??4FText@@QEAAAEAV0@AEBV0@@Z");a.textDestroy=symbol<decltype(a.textDestroy)>(core,"??1FText@@QEAA@XZ");
 a.copyValue=symbol<decltype(a.copyValue)>(objects,"?CopyCompleteValue@FProperty@@QEBAXPEAXPEBX@Z");
 a.setFocus=symbol<decltype(a.setFocus)>(umg,"?SetFocus@UWidget@@QEAAXXZ");
 a.focused=symbol<decltype(a.focused)>(umg,"?HasAnyUserFocus@UWidget@@QEBA_NXZ");a.focusedChildren=symbol<decltype(a.focusedChildren)>(umg,"?HasFocusedDescendants@UWidget@@QEBA_NXZ");a.scrollIntoView=symbol<decltype(a.scrollIntoView)>(umg,"?ScrollWidgetIntoView@UScrollBox@@QEAAXPEAVUWidget@@_NW4EDescendantScrollDestination@@M@Z");a.syncSwitcherSlot=symbol<decltype(a.syncSwitcherSlot)>(umg,"?SynchronizeProperties@UWidgetSwitcherSlot@@UEAAXXZ");a.syncScroll=symbol<decltype(a.syncScroll)>(umg,"?SynchronizeProperties@UScrollBox@@UEAAXXZ");
 a.arrayType=symbol<decltype(a.arrayType)>(objects,"?GetCPPType@FArrayProperty@@UEBA?AVFString@@PEAV2@I@Z");
 a.arrayClear=symbol<decltype(a.arrayClear)>(engine,"?GenericArray_Clear@UKismetArrayLibrary@@SAXPEAXPEBVFArrayProperty@@@Z");a.arrayAdd=symbol<decltype(a.arrayAdd)>(engine,"?GenericArray_Add@UKismetArrayLibrary@@SAHPEAXPEBVFArrayProperty@@PEBX@Z");
 a.language=reinterpret_cast<decltype(a.language)>(GetProcAddress(engine,"?GetCurrentLanguage@UKismetInternationalizationLibrary@@SA?AVFString@@XZ"));a.freeMemory=symbol<decltype(a.freeMemory)>(core,"?Free@FMemory@@SAXPEAX@Z");
}
char* item(int index){int count=read<int>(a.objects,0x24);if(index<0||index>=count||count>2000000)return nullptr;auto chunks=read<char**>(a.objects,0x10);auto chunk=chunks[index/65536];return chunk?chunk+(index%65536)*24:nullptr;}
bool validObject(Obj object,int index,int serial){auto slot=item(index);return slot&&read<Obj>(slot,0)==object&&read<int>(slot,16)==serial&&!(read<unsigned>(slot,8)&0x30000000);}
bool validController(){return validObject(controller,controllerIndex,controllerSerial);}
void currentLanguage(){if(!a.language)return;Str text;a.language(&text);if(text.data){language.clear();for(int i=0;i<text.count&&text.data[i];++i)language+=static_cast<char>(text.data[i]);a.freeMemory(text.data);}}
void copyProperty(Obj target,Obj source,const wchar_t* key){auto targetProperty=prop(target,key),sourceProperty=prop(source,key);a.copyValue(targetProperty,static_cast<char*>(target)+a.offset(targetProperty),static_cast<char*>(source)+a.offset(sourceProperty));}
void buttonStyle(Obj target,Obj source){for(auto key:{L"Style",L"Opacity",L"IsButton",L"ToggleTopLine",L"ToggleBottomLine",L"bIsTabButton",L"bIsSetting",L"ExpectedPreviewState",L"FocusKeeperTab"})copyProperty(target,source,key);}
void scrollStyle(Obj target,Obj source){for(auto key:{L"WidgetStyle",L"WidgetBarStyle",L"ScrollbarThickness",L"ScrollbarPadding",L"ScrollBarVisibility",L"NavigationDestination",L"NavigationScrollPadding",L"ScrollWhenFocusChanges"})copyProperty(target,source,key);}
void pageLayout(Obj target,Obj source){auto targetSlot=field(target,L"Slot",40),sourceSlot=field(source,L"Slot",40);for(auto key:{L"Padding",L"HorizontalAlignment",L"VerticalAlignment"})copyProperty(targetSlot,sourceSlot,key);a.syncSwitcherSlot(targetSlot);}
void followFocus(){Obj focused=nullptr;for(auto& row:rows)if(a.focused(row.widget)||a.focusedChildren(row.widget)){focused=row.widget;break;}if(focused&&focused!=lastFocused)a.scrollIntoView(page,focused,true,0,12.f);lastFocused=focused;}
bool attachable(Obj target){auto owner=field(target,L"OwningSysSettingsMenu",2000),left=field(target,L"LeftHandVBox",2016),pages=field(target,L"VerticalOptionSwitcher",2024);return owner&&left&&pages&&a.count(left)==4&&a.count(pages)==4;}
void populateValues(Obj spinner,const GyroOption& option,const Settings& settings){
  auto values=prop(spinner,L"Values");Str type,extended;a.arrayType(values,&type,&extended,0);std::wstring arrayType=(type.data?type.data:L"");if(extended.data)arrayType+=extended.data;if(type.data)a.freeMemory(type.data);if(extended.data)a.freeMemory(extended.data);
  bool strings=arrayType.find(L"FString")!=std::wstring::npos,texts=arrayType.find(L"FText")!=std::wstring::npos;
  if(!strings&&!texts)throw std::runtime_error("Unsupported native spinner value type");
  auto data=static_cast<char*>(spinner)+a.offset(values);a.arrayClear(data,values);
  bool managed=labelsExternalCalibration&&std::string_view(option.key)=="AutomaticCalibration";
  for(int i=0;i<(managed?1:nativeOptionCount(settings,option,labelsButtons));++i){auto label=managed?localize("calibration.steam",language):nativeOptionValue(settings,option,i,language,labelsLayout,labelsButtons);if(texts){ScopedText value(label);a.arrayAdd(data,values,&value.value);}else{auto w=wide(label);Str value{w.data(),static_cast<int>(w.size()+1),static_cast<int>(w.size()+1)};a.arrayAdd(data,values,&value);}}
}
void attachCalibrationPrompt(Obj target){
 auto reference=field(root,L"Prompt_ReturnDefault",2184),parent=a.parent(reference);
 if(!reference||!parent)throw std::runtime_error("Native calibration footer unavailable");
 calibrationPrompt=a.createWidget(target,read<Obj>(reference,0x10),newName(L"ReturnalGyroCalibration"));
 if(!calibrationPrompt)throw std::runtime_error("Cannot create calibration footer");
 for(auto key:{L"WidgetStyle",L"WidgetStyle_Pressed",L"WidgetStyle_Mouse",L"WidgetStyle_Text",L"IsNavBarPrompt",L"LargeText",L"SetBackingVisibility",L"IsMetaStyle"})copyProperty(calibrationPrompt,reference,key);
 write<Obj>(calibrationPrompt,offset(calibrationPrompt,L"OwnerScreen",1832),root);
 write<Name>(calibrationPrompt,offset(calibrationPrompt,L"InputAction",1976),0);
 write<Name>(calibrationPrompt,offset(calibrationPrompt,L"InputAxis",1984),0);
 write<bool>(calibrationPrompt,offset(calibrationPrompt,L"UseSetKey",2248),true);
 write<bool>(calibrationPrompt,offset(calibrationPrompt,L"IsMouseInteractionEnabled",2137),true);
 auto slot=a.addChild(parent,calibrationPrompt);if(!slot)throw std::runtime_error("Cannot attach calibration footer");
 auto sourceSlot=field(reference,L"Slot",40);
 for(auto key:{L"Padding",L"Size",L"HorizontalAlignment",L"VerticalAlignment"})copyProperty(slot,sourceSlot,key);
 bool designTime=false;call(calibrationPrompt,L"PreConstruct",&designTime,1);
 call(calibrationPrompt,L"SetKey",a.triangle,24);
 calibrationLabel.clear();calibrationWasDown=true;calibrationCountdown.cancel();calibrationWaiting=calibrationCollecting=false;calibrationCompletedAt=0;
 a.visibility(calibrationPrompt,1); // Collapsed until the gyro category is active.
}
void calibrationTick(Runtime& r){
 if(!calibrationPrompt)return;
 if(r.externalCalibration.load(std::memory_order_acquire)){calibrationCountdown.cancel();calibrationWaiting=calibrationCollecting=false;calibrationCompletedAt=0;calibrationWasDown=true;calibrationPromptActive=false;a.visibility(calibrationPrompt,1);return;}
 bool pageActive=a.inViewport(root)&&a.visible(root)&&read<int>(root,2088)==0&&read<int>(controller,2032)==4;
 if(pageActive!=calibrationPromptActive){calibrationPromptActive=pageActive;r.log(pageActive?"Native calibration footer active":"Native calibration footer hidden");}
 DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
 auto now=GetTickCount64();bool eligible=pageActive&&foreground==GetCurrentProcessId();
 a.visibility(calibrationPrompt,pageActive?0:1);
 // The added prompt is not registered in the game's action map; read the sensor device directly.
 bool down=(r.controllerButtons.load(std::memory_order_acquire)&buttonMask(11))!=0||read<bool>(calibrationPrompt,offset(calibrationPrompt,L"bIsMouseDown",2193));
 auto sampleAt=r.sampleTime.load();auto sensorNow=monotonicNs();bool connected=sampleAt&&sensorNow>=sampleAt&&sensorNow-sampleAt<200'000'000;
 MotionDiagnostics diagnostics;{std::lock_guard lock(r.diagnosticsMutex);diagnostics=r.diagnostics;}
 if(eligible&&connected&&down&&!calibrationWasDown&&!calibrationCountdown.active()&&!calibrationWaiting&&diagnostics.calibration!=CalibrationState::Collecting){
  calibrationCountdown.start(now);calibrationCompletedAt=0;r.log("Native calibration countdown started: 5 seconds");
 }
 calibrationWasDown=down;
 if(calibrationCountdown.tick(now,eligible&&connected)){r.calibrationCommand=1;calibrationWaiting=true;calibrationCollecting=false;calibrationRequestedAt=now;}
 if(calibrationWaiting){
  if(diagnostics.calibration==CalibrationState::Collecting)calibrationCollecting=true;
  if(calibrationCollecting&&diagnostics.calibration==CalibrationState::Complete){calibrationWaiting=false;calibrationCompletedAt=now;}
  else if(!connected||now-calibrationRequestedAt>30000)calibrationWaiting=false;
 }
 if(!pageActive){calibrationWasDown=true;return;}
 std::string label=localize("calibration.action",language);
 if(!connected)label=localize("calibration.unavailable",language);
 else if(calibrationCountdown.active())label=localize("calibration.countdown",language)+" "+std::to_string(calibrationCountdown.seconds(now))+" s";
 else if(calibrationWaiting||diagnostics.calibration==CalibrationState::Collecting)label=localize("calibration.collecting",language);
 else if(calibrationCompletedAt&&now-calibrationCompletedAt<3000)label=localize("calibration.complete",language);
 if(label!=calibrationLabel){ScopedText value(label);call(calibrationPrompt,L"SetPrompt",&value.value,sizeof(value.value));calibrationLabel=label;}
}
void buildRows(Obj target,const Settings& settings){
 auto rowClass=findClass(L"/Game/UI/SystemMenu/WBP_SettingBase.WBP_SettingBase_C"),spinnerClass=findClass(L"/Game/UI/SystemMenu/WBP_SettingSpinner.WBP_SettingSpinner_C");
 if(!rowClass||!spinnerClass)throw std::runtime_error("Native setting templates are not loaded");
 for(const auto* optionPtr:nativeVisibleOptions(labelsButtons)){
  const auto& option=*optionPtr;
  auto id=wide(option.key);auto rowName=L"ReturnalGyroRow_"+id,spinnerName=L"ReturnalGyroValue_"+id;
  Obj row=a.createWidget(target,rowClass,newName(rowName.c_str())),spinner=a.createWidget(target,spinnerClass,newName(spinnerName.c_str()));if(!row||!spinner)throw std::runtime_error("Cannot create native gyro row");
  write<Obj>(row,offset(row,L"OwningSysSettingsTab",1832),target);write<Obj>(row,offset(row,L"OwningSysSettingsMenu",1840),root);
  setText(row,L"Title",localize(option.key,language));setText(row,L"BodyTitle","");setText(row,L"BodyDescription",localize(labelsExternalCalibration&&std::string_view(option.key)=="AutomaticCalibration"?"calibration.steamHelp":std::string("description.")+option.key,language));
  populateValues(spinner,option,settings);
  int index=labelsExternalCalibration&&std::string_view(option.key)=="AutomaticCalibration"?0:nativeOptionIndex(settings,option,labelsButtons);write<int>(spinner,offset(spinner,L"SelectedIndex",2024),index);write<int>(spinner,offset(spinner,L"DefaultValue",2200),nativeOptionIndex(Settings{},option,labelsButtons));
  auto slot=field(row,L"SettingValue",2040);if(!slot||!a.setContent(slot,spinner)||!a.addChild(page,row))throw std::runtime_error("Cannot attach gyro row");
  call(spinner,L"SetSelectedIndexWithoutEnabledCheck",&index,sizeof(index));
  rows.push_back({row,spinner,&option,index});
 }
}
void refreshRows(Obj target,const Settings& settings){
 auto desired=nativeVisibleOptions(labelsButtons);bool same=desired.size()==rows.size();
 if(same)for(size_t i=0;i<rows.size();++i)if(rows[i].option!=desired[i]){same=false;break;}
 if(!same){
  // Returnal navigates panel child indices even for collapsed widgets. Only supported
  // options may be children; retaining hidden rows would leave invisible focus stops.
  bool active=read<int>(target,2032)==4,hadFocus=false,cachedOurs=false;
  std::string anchor;Obj cached=field(target,L"LastFocusedWidget",2536);
  int oldIndex=read<int>(target,offset(target,L"ScrollBarIndex",2036));
  if(active&&oldIndex>=0&&oldIndex<static_cast<int>(rows.size()))anchor=rows[oldIndex].option->key;
  for(auto& row:rows){if(cached==row.widget||cached==row.spinner){cachedOurs=true;if(!active)anchor=row.option->key;}
   if(a.focused(row.widget)||a.focusedChildren(row.widget)){hadFocus=true;anchor=row.option->key;}}
  for(auto& row:rows)if(!a.removeChild(page,row.widget))throw std::runtime_error("Cannot remove obsolete gyro row");
  rows.clear();lastFocused=nullptr;buildRows(target,settings);
  int selected=nativeFocusIndex(anchor,labelsButtons);
  if(active)write<int>(target,offset(target,L"ScrollBarIndex",2036),selected);
  if(cachedOurs)write<Obj>(target,offset(target,L"LastFocusedWidget",2536),rows[selected].widget);
  if(hadFocus&&active)a.setFocus(rows[selected].widget);
 }else for(auto& row:rows)if(optionFamily(*row.option)){
  int selected=nativeOptionIndex(settings,*row.option,labelsButtons);row.index=selected;
  populateValues(row.spinner,*row.option,settings);call(row.spinner,L"SetSelectedIndexWithoutEnabledCheck",&selected,sizeof(selected));
 }
}
void attach(Runtime& r,Obj target){
 currentLanguage();auto left=field(target,L"LeftHandVBox",2016),switcher=field(target,L"VerticalOptionSwitcher",2024),tree=field(target,L"WidgetTree",424);root=field(target,L"OwningSysSettingsMenu",2000);
 if(!left||!switcher||!tree||!root||a.count(left)!=4||a.count(switcher)!=4)throw std::runtime_error("Native menu requires the verified four-category Controls page");
 auto buttonClass=findClass(L"/Game/UI/SystemMenu/WBP_SettingsTabBtn.WBP_SettingsTabBtn_C"),rowClass=findClass(L"/Game/UI/SystemMenu/WBP_SettingBase.WBP_SettingBase_C"),spinnerClass=findClass(L"/Game/UI/SystemMenu/WBP_SettingSpinner.WBP_SettingSpinner_C");if(!buttonClass||!rowClass||!spinnerClass)throw std::runtime_error("Native menu templates are not loaded");
 page=a.construct(a.scrollClass(),tree,newName(L"ReturnalGyroPage"),0x40,0,nullptr,false,nullptr,false);if(!page)throw std::runtime_error("Cannot create gyro page");
 scrollStyle(page,field(target,L"SetupSettingsScrollBox",2264));
 auto settings=r.snapshot();labelsLayout=r.controllerLayout.load();labelsTouchpad=r.controllerTouchpad.load();labelsButtons=r.availableButtons.load();labelsExternalCalibration=r.externalCalibration.load();rows.clear();
 buildRows(target,settings);
 button=a.createWidget(target,buttonClass,newName(L"ReturnalGyroTab"));if(!button)throw std::runtime_error("Cannot create gyro category");
 buttonStyle(button,field(target,L"SetupButton",2256));
 write<Obj>(button,offset(button,L"OwningSysSettingsTab",1832),target);write<Obj>(button,offset(button,L"OwningSysSettingsMenu",1840),root);
 setText(button,L"Text",localize("page",language));setText(button,L"BodyTitle","");
 write<bool>(button,offset(button,L"bIsTabButton",1994),false);write<bool>(button,offset(button,L"bIsSetting",1993),false);
 if(!a.addChild(switcher,page))throw std::runtime_error("Cannot attach gyro page");
 if(!a.addChild(left,button)){a.removeChild(switcher,page);throw std::runtime_error("Cannot attach gyro category");}
 pageLayout(page,field(target,L"SetupSettingsScrollBox",2264));
 controller=target;controllerIndex=read<int>(target,12);controllerSerial=read<int>(item(controllerIndex),16);
 attachCalibrationPrompt(target);
 r.log("Native Gyro Configuration attached: "+std::to_string(rows.size())+" supported rows and calibration footer; language="+language);
}
}
void nativeMenuTick(Runtime& r){if(failed)return;try{
 if(!initialized){init();initialized=true;}
 if(pendingController){
  if(!validObject(pendingController,pendingIndex,pendingSerial))pendingController=nullptr;
  else if(attachable(pendingController)){auto target=pendingController;pendingController=nullptr;attach(r,target);}
 }
 if(controller&&!validController()){controller=nullptr;rows.clear();lastFocused=nullptr;calibrationPrompt=nullptr;calibrationCountdown.cancel();}
 if(!controller){if(GetTickCount64()<nextScan)return;nextScan=GetTickCount64()+2000;auto cls=findClass(L"/Game/UI/SystemMenu/WBP_ControllerSettings.WBP_ControllerSettings_C");if(!cls)return;int count=read<int>(a.objects,0x24);for(int i=0;i<count;++i){auto slot=item(i);if(!slot||(read<unsigned>(slot,8)&0x30000000))continue;auto obj=read<Obj>(slot,0);if(!obj||(read<unsigned>(obj,8)&0x30)||read<Obj>(obj,0x10)!=cls)continue;auto owner=field(obj,L"OwningSysSettingsMenu",2000),left=field(obj,L"LeftHandVBox",2016),pages=field(obj,L"VerticalOptionSwitcher",2024);if(!owner||!left||!pages||a.count(left)!=4||a.count(pages)!=4)continue;attach(r,obj);break;}return;}
 followFocus();calibrationTick(r);
 auto settings=r.snapshot();auto layout=r.controllerLayout.load();bool touchpad=r.controllerTouchpad.load();auto available=r.availableButtons.load();
 if(layout!=labelsLayout||touchpad!=labelsTouchpad||available!=labelsButtons){labelsLayout=layout;labelsTouchpad=touchpad;labelsButtons=available;refreshRows(controller,settings);}
 bool external=r.externalCalibration.load(std::memory_order_acquire);
 if(external!=labelsExternalCalibration){labelsExternalCalibration=external;for(auto& row:rows)if(std::string_view(row.option->key)=="AutomaticCalibration"){
  populateValues(row.spinner,*row.option,settings);int selected=external?0:nativeOptionIndex(settings,*row.option,labelsButtons);row.index=selected;call(row.spinner,L"SetSelectedIndexWithoutEnabledCheck",&selected,sizeof(selected));
  setText(row.widget,L"BodyDescription",localize(external?"calibration.steamHelp":"description.AutomaticCalibration",language));
 }}
 bool changed=false;for(auto& row:rows){int index=read<int>(row.spinner,2024);
 if(labelsExternalCalibration&&std::string_view(row.option->key)=="AutomaticCalibration"){if(index!=0){int zero=0;call(row.spinner,L"SetSelectedIndexWithoutEnabledCheck",&zero,sizeof(zero));}row.index=0;continue;}
if(index!=row.index&&index>=0&&index<nativeOptionCount(settings,*row.option,labelsButtons)){
 setNativeOptionIndex(settings,*row.option,index,labelsButtons);row.index=index;changed=true;

}else{int desired=nativeOptionIndex(settings,*row.option,labelsButtons);if(desired!=row.index){call(row.spinner,L"SetSelectedIndexWithoutEnabledCheck",&desired,sizeof(desired));row.index=desired;}}}
 if(changed){r.update(settings);r.log("Native gyro options changed");}
 }catch(const std::exception& e){failed=true;r.log(e.what());}}
#ifdef RG_NATIVE_MENU_DIAGNOSTIC
// Development-only repair of the already attached preview; never used by the production DLL.
void nativeMenuRepairTick(Runtime& r){if(failed)return;try{
 if(!initialized){init();initialized=true;}
 if(controller&&!validController()){controller=nullptr;rows.clear();lastFocused=nullptr;calibrationPrompt=nullptr;calibrationCountdown.cancel();}
 if(!controller){if(GetTickCount64()<nextScan)return;nextScan=GetTickCount64()+2000;auto cls=findClass(L"/Game/UI/SystemMenu/WBP_ControllerSettings.WBP_ControllerSettings_C");if(!cls)return;
 int count=read<int>(a.objects,0x24);for(int i=0;i<count;++i){auto entry=item(i);if(!entry||(read<unsigned>(entry,8)&0x30000000))continue;auto obj=read<Obj>(entry,0);if(!obj||(read<unsigned>(obj,8)&0x30)||read<Obj>(obj,0x10)!=cls)continue;
 auto left=field(obj,L"LeftHandVBox",2016),switcher=field(obj,L"VerticalOptionSwitcher",2024);if(!left||!switcher||a.count(left)!=5||a.count(switcher)!=5)continue;
 button=a.child(left,4);page=a.child(switcher,4);if(a.count(page)!=14||read<Obj>(button,0x10)!=findClass(L"/Game/UI/SystemMenu/WBP_SettingsTabBtn.WBP_SettingsTabBtn_C"))continue;
 currentLanguage();buttonStyle(button,a.child(left,3));setText(button,L"Text",localize("page",language));setText(button,L"BodyTitle","");bool designTime=false;call(button,L"PreConstruct",&designTime,1);
 scrollStyle(page,a.child(switcher,3));a.syncScroll(page);pageLayout(page,a.child(switcher,3));rows.clear();for(int index=0;index<14;++index){auto row=a.child(page,index);const auto& option=gyroOptions()[index];ScopedText title(localize(option.key,language));call(row,L"Set Title Text",&title.value,sizeof(title.value));setText(row,L"BodyTitle","");rows.push_back({row});}
 controller=obj;controllerIndex=read<int>(obj,12);controllerSerial=read<int>(entry,16);r.log("Native preview category mouse-focus contract restored");break;}return;}
 followFocus();
 }catch(const std::exception& e){failed=true;r.log(e.what());}}

#endif

namespace {
void(*originalDraw)(Obj,Obj,Obj){};void(*originalRestore)(Obj){};void(*originalConstruct)(Obj){};
void menuDraw(Obj viewport,Obj target,Obj canvas){nativeMenuTick(*runtime);originalDraw(viewport,target,canvas);}
void constructed(Obj widget){
 originalConstruct(widget);
 if(failed)return;
 try{
  if(!initialized){init();initialized=true;}
  auto cls=read<Obj>(widget,0x10);Obj candidate=nullptr;
  if(cls==findClass(L"/Game/UI/SystemMenu/WBP_ControllerSettings.WBP_ControllerSettings_C"))candidate=widget;
  else if(cls==findClass(L"/Game/UI/SystemMenu/WBP_SystemSettings_3Panel.WBP_SystemSettings_3Panel_C"))candidate=field(widget,L"WBP_ControllerSettings",2336);
  if(candidate&&!(read<unsigned>(candidate,8)&0x30)){
   int index=read<int>(candidate,12);auto slot=item(index);
   if(slot){pendingController=candidate;pendingIndex=index;pendingSerial=read<int>(slot,16);nextScan=0;}
  }
 }catch(const std::exception& error){failed=true;runtime->log(error.what());}
}
void restore(Obj target){
 if(!failed&&controller&&target==controller&&validController()&&read<int>(controller,2032)==4){
  auto settings=runtime->snapshot();resetGyroOptions(settings);runtime->update(settings);
  try{for(auto& row:rows){populateValues(row.spinner,*row.option,settings);int index=nativeOptionIndex(settings,*row.option,labelsButtons);call(row.spinner,L"SetSelectedIndexWithoutEnabledCheck",&index,sizeof(index));row.index=index;}runtime->log("Native gyro defaults restored");}
  catch(const std::exception& error){failed=true;runtime->log(error.what());}
  return;
 }
 originalRestore(target);
}
}
bool installNativeMenu(Runtime& r){
 // Engine, Core, CoreUObject, UMG and Returnal hashes are validated before this call.
 try{
  auto engine=GetModuleHandleW(L"Returnal-Engine-Win64-Shipping.dll"),game=GetModuleHandleW(L"Returnal-Returnal-Win64-Shipping.dll");
  auto drawTarget=symbol<void*>(engine,"?Draw@UGameViewportClient@@UEAAXPEAVFViewport@@PEAVFCanvas@@@Z");
  auto restoreTarget=symbol<void*>(game,"?RestoreToDefault@USysSettingsTab@@QEAAXXZ");
  auto constructTarget=symbol<void*>(GetModuleHandleW(L"Returnal-UMG-Win64-Shipping.dll"),"?NativeConstruct@UUserWidget@@MEAAXXZ");
  struct Hook{void* target;void* detour;void** original;};
  Hook hooks[]={{drawTarget,reinterpret_cast<void*>(menuDraw),reinterpret_cast<void**>(&originalDraw)},
   {restoreTarget,reinterpret_cast<void*>(restore),reinterpret_cast<void**>(&originalRestore)},
   {constructTarget,reinterpret_cast<void*>(constructed),reinterpret_cast<void**>(&originalConstruct)}};
  size_t created=0;for(auto& hook:hooks){if(MH_CreateHook(hook.target,hook.detour,hook.original)!=MH_OK){for(size_t i=0;i<created;++i)MH_RemoveHook(hooks[i].target);throw std::runtime_error("Native menu hook creation failed");}++created;}
  for(auto& hook:hooks)MH_QueueEnableHook(hook.target);
  if(MH_ApplyQueued()!=MH_OK){for(auto& hook:hooks)MH_DisableHook(hook.target);throw std::runtime_error("Native menu hook activation failed");}
  r.log("Native Controls / Gyro Configuration integration installed; construction-triggered attachment");return true;
 }catch(const std::exception& error){r.log(error.what());return false;}
}

}
