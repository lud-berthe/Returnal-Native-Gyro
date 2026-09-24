#include "rg/calibration_policy.hpp"
#include "rg/controller_presentation.hpp"
#include "rg/gyro_menu.hpp"
#include "rg/motion.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
void check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
void near(float a,float b,float tolerance,const char* msg){check(std::abs(a-b)<tolerance,msg);}
struct Stream {
 rg::GyroSample sample;int automaticEvents{},manualEvents{};
 void run(rg::MotionProcessor& p,const rg::Settings& s,bool menu,int ticks=7500){
  for(int i=0;i<ticks;++i){sample.sensorNs+=4'000'000;auto d=p.process(sample,s,{!menu,false,false,false,false,menu});automaticEvents+=p.diagnostics().calibrationEvent==rg::CalibrationEvent::AutomaticCorrection;manualEvents+=p.diagnostics().calibrationEvent==rg::CalibrationEvent::ManualComplete;if(menu)check(d.yawDegrees==0&&d.pitchDegrees==0,"menu calibration never moves camera");}
 }
};
int main(){try{
 const auto& option=rg::gyroOptions()[11];
 check(std::string_view(option.key)=="AutomaticCalibration"&&option.count==3,"three direct calibration choices");
 for(int mode=0;mode<3;++mode){
  auto config=rg::parseConfig("ConfigVersion=3\nAutomaticCalibration="+std::to_string(mode)+"\nSensitivityX=6\nSensitivityY=4\n");
  check(config.warnings.empty()&&config.settings.AutomaticCalibration==mode,"old and new INI values supported");
  check(config.settings.SensitivityX==6&&config.settings.SensitivityY==4,"other settings preserved");
  check(rg::parseConfig(rg::serializeConfig(config.settings)).settings.AutomaticCalibration==mode,"calibration persists");
  check(rg::optionIndex(config.settings,option)==(mode==1?2:mode==2?1:0),"legacy enabled remains Anytime with new UI ordering");
  for(auto lang:{"en","fr","de","es","it","pt"})check(rg::optionValue(option,rg::optionIndex(config.settings,option),lang).find("calibration.")==std::string::npos,"translated calibration values");
 }
 check(rg::parseConfig("AutomaticCalibration=3").settings.AutomaticCalibration==0,"invalid mode restores Off");
 rg::Settings selected;selected.SensitivityX=6;selected.SensitivityY=4;
 for(int backend=0;backend<=4;++backend){
  selected.SensorBackend=backend;selected.SensitivityX=6;selected.SensitivityY=4;
  for(int index:{0,1,2,0}){
   rg::setNativeOptionIndex(selected,option,index,rg::standardGyroButtons);
   check(selected.SensorBackend==backend,"calibration never changes source, including legacy saved choices");
   check(rg::nativeOptionIndex(selected,option,rg::standardGyroButtons)==index,"direct calibration choice round trip");
   check(selected.AutomaticCalibration==(index==1?2:index==2?1:0),"direct calibration mapping");
   check(selected.SensitivityX==6&&selected.SensitivityY==4,"calibration preserves sensitivity");
  }
  rg::resetGyroOptions(selected);check(selected.SensorBackend==backend&&selected.AutomaticCalibration==0,"restore defaults cannot change source");
 }
 // Values follow the existing sentence-case menu style, including the brand name.
 check(rg::optionValue(option,1,"fr")=="Menus uniquement"&&rg::optionValue(option,2,"fr")=="À tout moment","French calibration values use normal case");
 check(rg::localize("calibration.steam","fr")=="Steam Input","Steam calibration label uses brand casing");
 rg::Settings s;s.ActivationMode=0;s.GyroSpace=0;s.AutomaticCalibration=2;
 // Regression: actor ticks stop for the entire paused menu while UI frames
 // keep publishing visibility. Bias must still learn, then remain fixed on close.
 {
  auto paused=std::make_unique<rg::MotionProcessor>();Stream stream;
  stream.sample.accelG={0,1,0};stream.sample.degreesPerSecond={.2f,-.4f,.1f};
  std::uint64_t now=10'000'000'000,controllerAt=now-1'000'000'000,uiAt=0;
  for(int i=0;i<7500;++i){
   now+=4'000'000;if(i%4==0)uiAt=now;
   bool menu=rg::calibrationMenuContext(controllerAt,uiAt,now);
   check(menu,"paused UI heartbeat survives stopped actor ticks");
   stream.run(*paused,s,menu,1);
  }
  near(paused->diagnostics().bias.y,-.4f,.03f,"paused menu learns drift from UI context");
  auto bias=paused->diagnostics().bias;
  uiAt=0;check(!rg::calibrationMenuContext(controllerAt,uiAt,now),"closing UI clears menu despite old actor snapshot");
  stream.sample.degreesPerSecond={1,-1,1};stream.run(*paused,s,false);
  near(paused->diagnostics().bias.y,bias.y,.00001f,"menu closure locks learned drift");
  check(!rg::calibrationMenuContext(controllerAt,now,now+100'000'000),"stalled UI heartbeat expires");
  check(rg::calibrationMenuContext(now,0,now),"live controller menu remains supported");
  check(!rg::calibrationMenuContext(0,0,now),"loading without menu evidence does not calibrate");
 }
 check(rg::calibrationMenuWidgetEligible(true,true,true,true,false),"visible native menu qualifies");
 check(!rg::calibrationMenuWidgetEligible(false,true,true,true,false),"destroyed menu cannot qualify");
 check(!rg::calibrationMenuWidgetEligible(true,false,true,true,false),"hidden menu cannot qualify");
 check(!rg::calibrationMenuWidgetEligible(true,true,false,true,false),"removed menu cannot qualify");
 check(!rg::calibrationMenuWidgetEligible(true,true,true,false,false),"background menu cannot qualify");
 check(!rg::calibrationMenuWidgetEligible(true,true,true,true,true),"suspended mod cannot qualify");
 auto p=std::make_unique<rg::MotionProcessor>();Stream f;f.sample.accelG={0,1,0};f.sample.degreesPerSecond={.2f,-.4f,.1f};
 f.run(*p,s,false);near(p->diagnostics().bias.y,0,.0001f,"Menus Only never learns slow gameplay motion");check(f.automaticEvents==0,"no debug correction event outside allowed calibration");
 // Loss of gameplay control alone must not grant menu calibration.
 for(int i=0;i<7500;++i){f.sample.sensorNs+=4'000'000;p->process(f.sample,s,{false,false,false,true,false,false});}
 near(p->diagnostics().bias.y,0,.0001f,"cinematic/loading/background are not menus");
 f.run(*p,s,true);near(p->diagnostics().bias.y,-.4f,.03f,"still menu controller learns drift");
 check(f.automaticEvents>0,"automatic bias correction emits debug event");
 auto events=f.automaticEvents;auto bias=p->diagnostics().bias;
 f.sample.degreesPerSecond={1,-1,1};f.run(*p,s,false);
 near(p->diagnostics().bias.y,bias.y,.00001f,"learned bias stays locked during gameplay");check(f.automaticEvents==events,"locked bias generates no correction notification");
 s.AutomaticCalibration=0;f.sample.degreesPerSecond={.1f,-.1f,.1f};f.run(*p,s,true);
 near(p->diagnostics().bias.y,bias.y,.00001f,"Off retains calibration even in menus");
 s.AutomaticCalibration=1;f.run(*p,s,false);near(p->diagnostics().bias.y,-.1f,.03f,"Anytime still calibrates in gameplay");
 s.AutomaticCalibration=2;
 auto moving=std::make_unique<rg::MotionProcessor>();Stream movement;movement.sample.accelG={0,1,0};
 for(int i=0;i<7500;++i){movement.sample.sensorNs+=4'000'000;movement.sample.degreesPerSecond={float(i%2?20:-20),float(i%2?-20:20),0};moving->process(movement.sample,s,{false,false,false,false,false,true});}
 near(moving->diagnostics().bias.y,0,.0001f,"moving controller in menu is not calibrated");
 // Entering/leaving a menu must not clear estimated gravity or stored bias.
 for(int i=0;i<20;++i){f.run(*p,s,i%2!=0,1);auto g=p->diagnostics().gravity;check(g.x*g.x+g.y*g.y+g.z*g.z>.9f,"menu edges preserve gravity");}
 for(int mode=0;mode<3;++mode){
  auto steam=std::make_unique<rg::MotionProcessor>();steam->setExternalCalibration(true);s.AutomaticCalibration=mode;Stream slow;slow.sample.degreesPerSecond={0,-1,0};
  steam->beginCalibration();steam->resetCalibration();slow.run(*steam,s,true);slow.run(*steam,s,false);
  check(slow.automaticEvents==0&&slow.manualEvents==0,"Steam never fabricates calibration events");near(steam->diagnostics().bias.y,0,.00001f,"Steam calibration never supplemented");near(steam->diagnostics().calibrated.y,-1,.00001f,"Steam slow tracking retained in every mode");
 }
 p->beginCalibration();s.AutomaticCalibration=2;s.CalibrationSeconds=1;f.sample.degreesPerSecond={.1f,-.2f,.3f};f.run(*p,s,true,300);
 check(p->diagnostics().calibration==rg::CalibrationState::Complete,"manual calibration still completes");check(f.manualEvents==1,"manual completion produces exactly one debug event");near(p->diagnostics().bias.y,-.2f,.001f,"manual mean unaffected by automatic mode");
 rg::resetGyroOptions(s);check(s.AutomaticCalibration==0,"restore defaults returns Off");
 check(rg::freshCalibrationMenu(10,11)&&!rg::freshCalibrationMenu(0,11)&&!rg::freshCalibrationMenu(10,9)&&!rg::freshCalibrationMenu(10,100'000'010),"stale menu state fails closed");
 check(rg::calibrationPromptVisible(true,1,false)&&!rg::calibrationPromptVisible(true,0,false)&&!rg::calibrationPromptVisible(true,-1,false)&&!rg::calibrationPromptVisible(true,1,true)&&!rg::calibrationPromptVisible(false,1,false),"footer appears only for direct controller on gyro page");
 check(rg::calibrationIconVendor(rg::ControllerLayout::Sony)==1&&rg::calibrationIconVendor(rg::ControllerLayout::Nintendo)==5&&rg::calibrationIconVendor(rg::ControllerLayout::Steam)==3&&rg::calibrationIconVendor(rg::ControllerLayout::Generic)==-1,"supported native icon families and fallback");
 for(auto layout:{rg::ControllerLayout::Sony,rg::ControllerLayout::Nintendo,rg::ControllerLayout::Steam,rg::ControllerLayout::Xbox,rg::ControllerLayout::Generic}){
  auto kb=rg::controllerGlyphStyle(false,true,layout,true,3);check(kb.sony&&kb.vendor==3,"keyboard glyph calls unchanged");
  auto stale=rg::controllerGlyphStyle(true,false,layout,false,5);check(!stale.sony&&stale.vendor==5,"stale identity preserves native glyphs");
 }
 auto nintendo=rg::controllerGlyphStyle(true,true,rg::ControllerLayout::Nintendo,true,3);check(!nintendo.sony&&nintendo.vendor==5,"Steam XInput presentation uses actual Nintendo glyphs");
 using D=rg::ControllerDiagram;using L=rg::ControllerLayout;
 struct Example{int type;L layout;D diagram;};
 for(auto e:{Example{5,L::Sony,D::DualShock},Example{13,L::Sony,D::DualSense},Example{2,L::Xbox,D::Xbox360},Example{3,L::Xbox,D::Xbox},Example{10,L::Nintendo,D::SwitchPro},Example{1,L::Steam,D::Xbox},Example{18,L::Steam,D::Xbox}}){
  auto p=rg::steamControllerPresentation(e.type);check(p.layout==e.layout&&p.diagram==e.diagram,"Steam identity determines both glyph family and physical model");
  check(rg::controllerArtwork(p.diagram).guide&&rg::controllerArtwork(p.diagram).preview,"each known model has both native artwork paths");
 }
 check(std::wstring_view(rg::controllerArtwork(D::DualSense).guide)==L"WBP_SystemController_Guide","DualSense uses PS5 art");
 check(std::wstring_view(rg::controllerArtwork(D::DualShock).guide)==L"WBP_SystemController_Guide_DualShock","DualShock remains distinct from DualSense");
 check(!rg::controllerArtwork(rg::steamControllerPresentation(999).diagram).guide,"unknown model preserves native artwork");
 auto edge=rg::resolveSteamPresentation(5,0x054c,0x0df2,{L::Sony,D::DualShock},true);
 check(edge.layout==L::Sony&&edge.diagram==D::DualSense,"reported PS4 plus associated Edge hardware resolves DualSense");
 check(rg::resolveSteamPresentation(5,0x054c,0x0ce6,{},true).diagram==D::DualSense,"standard DualSense hardware resolves legacy Steam type");
 check(rg::resolveSteamPresentation(5,0x054c,0x09cc,{},true).diagram==D::DualShock,"real DualShock remains PS4");
 check(rg::resolveSteamPresentation(5,0x045e,0x0df2,{},true).diagram==D::DualShock,"product alone cannot identify Sony");
 check(rg::resolveSteamPresentation(5,0,0,{L::Sony,D::DualSense},true).diagram==D::DualSense,"associated SDL model refines Sony family");
 check(rg::resolveSteamPresentation(5,0x054c,0x0df2,{L::Sony,D::DualSense},false).diagram==D::DualShock,"unassociated hardware cannot override selected controller");
 check(rg::resolveSteamPresentation(10,0x054c,0x0df2,{L::Xbox,D::Xbox},true).diagram==D::SwitchPro,"Nintendo identity not overwritten by another family");
 check(rg::resolveSteamPresentation(13,0,0,{L::Xbox,D::Xbox360},false).diagram==D::DualSense,"virtual XInput fallback preserves Steam identity");

 std::cout<<"Menu calibration: bias learning/retention, motion rejection, Steam bypass, gravity preservation, config and footer policies passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
