#include "skin.h"
#include <d3d11.h>
#include <dxgi.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

namespace {
ID3D11Device* dev = nullptr;
ID3D11ShaderResourceView* srv = nullptr;
float asp = 1.f;
}

namespace Skin {
void init(void* swapchain, unsigned ver) {
    if (ver != 11 || !swapchain) return;                   // skins are DX11-only
    static_cast<IDXGISwapChain*>(swapchain)->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev));
}

bool load(const char* path) {
    if (!dev) return false;
    int w, h, n;
    unsigned char* px = stbi_load(path, &w, &h, &n, 4);
    if (!px) return false;
    D3D11_TEXTURE2D_DESC d{};
    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{px, (UINT)(w * 4), 0};
    ID3D11Texture2D* tex = nullptr;
    bool ok = SUCCEEDED(dev->CreateTexture2D(&d, &sd, &tex));
    stbi_image_free(px);
    if (!ok) return false;
    ID3D11ShaderResourceView* nv = nullptr;
    ok = SUCCEEDED(dev->CreateShaderResourceView(tex, nullptr, &nv));
    tex->Release();
    if (!ok) return false;
    if (srv) srv->Release();                               // NOTE: only call from the render thread if hot-reloading
    srv = nv; asp = (float)w / (float)h;
    return true;
}

void shutdown() { if (srv) { srv->Release(); srv = nullptr; } if (dev) { dev->Release(); dev = nullptr; } }
ImTextureID texture() { return (ImTextureID)(intptr_t)srv; }
float aspect() { return asp; }
}
