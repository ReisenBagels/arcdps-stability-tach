#pragma once
#include <imgui.h>
namespace Skin {
    void init(void* swapchain, unsigned d3dVersion);   // call once from mod_init
    bool load(const char* path);                        // (re)load a PNG/JPG; safe to call at runtime
    void shutdown();
    ImTextureID texture();                              // 0 if no skin loaded
    float aspect();                                     // width / height
}
