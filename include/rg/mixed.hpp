#pragma once
namespace rg {
enum class InputEventKind {MouseMotion,KeyboardOrMouseButton,Controller};
// Suppress only the presentation-state update, never the input event itself.
inline bool suppressPresentation(bool enabled,bool ignoreMouse,int hudMode,InputEventKind event) {
    if(!enabled)return false;
    if(hudMode==1)return event!=InputEventKind::Controller;
    if(hudMode==2)return event==InputEventKind::Controller;
    return ignoreMouse&&event==InputEventKind::MouseMotion;
}
}
