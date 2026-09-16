#pragma once
#include <algorithm>
namespace rg {
// Use both Windows DPI and monitor resolution: 4K at 100% scaling still needs readable text.
inline float panelScale(float requested,unsigned dpi,int monitorHeight){
    if(requested>0)return std::clamp(requested,1.0f,3.0f);
    return std::clamp(std::max(static_cast<float>(dpi)/96.0f,static_cast<float>(monitorHeight)/1080.0f),1.0f,3.0f);
}
}
