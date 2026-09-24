#pragma once
#include "controller_buttons.hpp"
namespace rg {
enum class ControllerDiagram { Native, DualSense, DualShock, Xbox, Xbox360, SwitchPro };
struct ControllerPresentation {ControllerLayout layout{ControllerLayout::Generic};ControllerDiagram diagram{ControllerDiagram::Native};};
constexpr ControllerPresentation steamControllerPresentation(int type){
 switch(type){
 case 5:case 12:return {ControllerLayout::Sony,ControllerDiagram::DualShock};
 case 13:return {ControllerLayout::Sony,ControllerDiagram::DualSense};
 case 8:case 9:case 10:case 16:return {ControllerLayout::Nintendo,ControllerDiagram::SwitchPro};
 case 2:return {ControllerLayout::Xbox,ControllerDiagram::Xbox360};
 case 3:return {ControllerLayout::Xbox,ControllerDiagram::Xbox};
 // Returnal contains no Steam Controller illustration; retain the Xbox-style
 // diagram matching the game's available Steam button glyphs.
 case 1:case 14:case 15:case 17:case 18:return {ControllerLayout::Steam,ControllerDiagram::Xbox};
 default:return {};
 }
}
// Steam's legacy interface may report a broad/older family (e.g. type 5 for
// a DualSense). Refine it only from the SDL gamepad associated with this exact
// Steam handle. A virtual XInput slot is never evidence of the physical model.
constexpr ControllerPresentation resolveSteamPresentation(int type,unsigned vendor,unsigned product,ControllerPresentation device,bool exactAssociation){
 auto steam=steamControllerPresentation(type);
 if(!exactAssociation)return steam;
 if(vendor==0x054c&&(steam.layout==ControllerLayout::Sony||steam.layout==ControllerLayout::Generic)){
  if(product==0x0ce6||product==0x0df2)return {ControllerLayout::Sony,ControllerDiagram::DualSense};
  if(product==0x05c4||product==0x09cc)return {ControllerLayout::Sony,ControllerDiagram::DualShock};
 }
 // Refine within a known family; never let Xbox emulation overwrite a Sony,
 // Nintendo or Steam identity. Unknown Steam families may use a known device.
 if(device.diagram!=ControllerDiagram::Native&&(steam.layout==ControllerLayout::Generic||steam.layout==device.layout))return device;
 return steam;
}
struct ControllerArtwork {const wchar_t* guide;const wchar_t* preview;};
constexpr ControllerArtwork controllerArtwork(ControllerDiagram diagram){
 switch(diagram){
 case ControllerDiagram::DualSense:return {L"WBP_SystemController_Guide",L"WBP_ControllerFocusPreview"};
 case ControllerDiagram::DualShock:return {L"WBP_SystemController_Guide_DualShock",L"WBP_ControllerFocusDualshockPreview"};
 case ControllerDiagram::Xbox:return {L"WBP_SystemController_Guide_Xbox",L"WBP_ControllerFocusXboxPreview"};
 case ControllerDiagram::Xbox360:return {L"WBP_SystemController_Guide_Xbox360",L"WBP_ControllerFocusXbox360Preview"};
 case ControllerDiagram::SwitchPro:return {L"WBP_SystemController_Guide_SwitchPro",L"WBP_ControllerFocusSwitchProPreview"};
 default:return {};
 }
}
}
