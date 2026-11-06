#include "skin.h"
#include "log.h"
#include <Windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <map>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

namespace fs = std::filesystem;

namespace {
const char* kBase = "addons/arcdps/stability_tach";
const char* kSkins = "addons/arcdps/stability_tach/skins";
ID3D11Device* dev = nullptr;
std::map<std::string, Skin::Info> cache;
std::map<std::string, ID3D11ShaderResourceView*> srvs;
struct Dead { ID3D11ShaderResourceView* v; int frames; };
std::vector<Dead> graveyard;              // released a few frames late: draw data of the current frame may still reference them
std::string devStatus = "not initialised";
int anchorVar = 0;                        // an address that lives inside this DLL

std::string dllDir() {
    HMODULE h = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&anchorVar), &h)) return "";
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(h, buf, MAX_PATH);
    return n ? fs::path(std::string(buf, n)).parent_path().string() : "";
}

std::vector<fs::path> candidates(const std::string& file) {
    std::vector<fs::path> c; fs::path f(file);
    if (f.is_absolute()) { c.push_back(f); return c; }
    c.push_back(fs::path(kSkins) / f);          // normal place
    c.push_back(fs::path(kBase) / f);           // where earlier versions looked
    c.push_back(f);                             // relative to the game folder
    std::string d = dllDir();                   // and relative to the DLL, in case the working directory is not the game folder
    if (!d.empty()) {
        c.push_back(fs::path(d) / "arcdps" / "stability_tach" / "skins" / f);
        c.push_back(fs::path(d) / "stability_tach" / "skins" / f);
        c.push_back(fs::path(d) / f);
    }
    return c;
}

bool createTexture(const fs::path& p, ID3D11ShaderResourceView*& out, int& w, int& h, std::string& err) {
    FILE* fp = _wfopen(p.wstring().c_str(), L"rb");       // wide-char open so non-ASCII folder names work
    if (!fp) { err = "could not open file"; return false; }
    int n = 0;
    unsigned char* px = stbi_load_from_file(fp, &w, &h, &n, 4);
    std::fclose(fp);
    if (!px) { err = std::string("could not decode image: ") + stbi_failure_reason(); return false; }
    D3D11_TEXTURE2D_DESC d{};
    d.Width = (UINT)w; d.Height = (UINT)h; d.MipLevels = 1; d.ArraySize = 1;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{px, (UINT)(w * 4), 0};
    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = dev->CreateTexture2D(&d, &sd, &tex);
    stbi_image_free(px);
    if (FAILED(hr) || !tex) { err = "CreateTexture2D failed (image too large?)"; return false; }
    hr = dev->CreateShaderResourceView(tex, nullptr, &out);
    tex->Release();
    if (FAILED(hr) || !out) { err = "CreateShaderResourceView failed"; return false; }
    return true;
}

void bury(ID3D11ShaderResourceView* v) { if (v) graveyard.push_back({v, 4}); }
}

namespace Skin {
void init(void* p) {
    std::error_code ec;
    fs::create_directories(kSkins, ec);
    if (!p) { devStatus = "ArcDPS gave no Direct3D pointer"; Log::write("SKIN init: %s", devStatus.c_str()); return; }
    // ArcDPS's last init argument is NOT the D3D version any more (we saw 19270), so ask the object itself.
    IUnknown* u = static_cast<IUnknown*>(p);
    IDXGISwapChain* sc = nullptr;
    if (SUCCEEDED(u->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&sc))) && sc) {
        if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev))) && dev) devStatus = "D3D11 device found (via swap chain)";
        sc->Release();
    }
    if (!dev && SUCCEEDED(u->QueryInterface(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev))) && dev) devStatus = "D3D11 device found (direct)";
    if (!dev) devStatus = "no D3D11 device behind ArcDPS's pointer (skins need DX11)";
    Log::write("SKIN init: %s", devStatus.c_str());
}

const std::string& deviceStatus() { return devStatus; }

void beginFrame() {
    for (auto& d : graveyard) d.frames--;
    graveyard.erase(std::remove_if(graveyard.begin(), graveyard.end(), [](Dead& d) { if (d.frames > 0) return false; d.v->Release(); return true; }), graveyard.end());
}

const Info& get(const std::string& file) {
    Info& inf = cache[file];
    if (inf.ok) return inf;
    if (file.empty()) { inf.status = "No skin file selected"; return inf; }
    const double now = ImGui::GetTime();
    if (now - inf.lastTry < 3.0) return inf;           // failed recently; try again shortly (picks up files dropped in later)
    inf.lastTry = now;
    if (!dev) { inf.status = "Cannot load: " + devStatus; return inf; }
    std::error_code ec; fs::path found;
    auto cands = candidates(file);
    for (auto& c : cands) if (fs::is_regular_file(c, ec)) { found = c; break; }
    if (found.empty()) {
        inf.status = "File not found: " + file + "  (put it in " + std::string(kSkins) + ")";
        Log::write("SKIN '%s': not found (tried %zu locations, first: %s)", file.c_str(), cands.size(), cands.front().string().c_str());
        return inf;
    }
    ID3D11ShaderResourceView* v = nullptr; int w = 0, h = 0; std::string err;
    if (!createTexture(found, v, w, h, err)) {
        inf.status = "Found " + found.string() + " but " + err;
        Log::write("SKIN '%s': %s", file.c_str(), inf.status.c_str());
        return inf;
    }
    bury(srvs[file]); srvs[file] = v;
    inf.tex = (ImTextureID)(intptr_t)v; inf.aspect = (float)w / (float)std::max(1, h); inf.ok = true;
    inf.status = "Loaded " + std::to_string(w) + "x" + std::to_string(h) + " from " + found.string();
    Log::write("SKIN '%s': %s", file.c_str(), inf.status.c_str());
    return inf;
}

void reload() {
    for (auto& kv : srvs) bury(kv.second);
    srvs.clear(); cache.clear();
}

std::vector<std::string> list() {
    std::vector<std::string> out; std::error_code ec;
    for (auto& e : fs::directory_iterator(kSkins, ec)) {
        if (!e.is_regular_file(ec)) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".gif") out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string skinsDir() { return kSkins; }

bool openFolder() {
    std::error_code ec;
    fs::create_directories(kSkins, ec);
    std::string abs = fs::absolute(kSkins, ec).string();
    return (INT_PTR)ShellExecuteA(nullptr, "open", abs.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}

void shutdown() {
    for (auto& kv : srvs) if (kv.second) kv.second->Release();
    for (auto& d : graveyard) d.v->Release();
    srvs.clear(); graveyard.clear(); cache.clear();
    if (dev) { dev->Release(); dev = nullptr; }
}
}
