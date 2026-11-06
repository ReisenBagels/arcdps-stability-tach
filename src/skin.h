#pragma once
#include <imgui.h>
#include <string>
#include <vector>

// Skin images: PNG/JPG/BMP/TGA/GIF files in addons/arcdps/stability_tach/skins (or any full path).
// Textures are created lazily on the render thread and cached per file name.
namespace Skin {
    struct Info {
        ImTextureID tex{};          // valid only when ok
        float aspect = 1.f;         // width / height
        bool ok = false;
        std::string status;         // human readable: what happened / where it looked
        double lastTry = -1e9;      // ImGui time of the last load attempt (failed loads are retried every few seconds)
    };
    void init(void* d3dPtr);                    // finds the D3D11 device from ArcDPS's pointer (does NOT trust the version argument)
    void beginFrame();                          // call once per frame: releases textures that are no longer in flight
    const Info& get(const std::string& file);   // loads on first use
    void reload();                              // forget everything so files are re-read from disk
    std::vector<std::string> list();            // image files found in the skins folder
    std::string skinsDir();
    bool openFolder();                          // opens the skins folder in Explorer
    const std::string& deviceStatus();
    void shutdown();
}
