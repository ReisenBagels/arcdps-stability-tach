#pragma once
#include <Windows.h>
typedef unsigned UINT;
enum { DXGI_FORMAT_R8G8B8A8_UNORM = 28, D3D11_USAGE_DEFAULT = 0, D3D11_BIND_SHADER_RESOURCE = 8 };
struct DXGI_SAMPLE_DESC { UINT Count; UINT Quality; };
struct D3D11_TEXTURE2D_DESC { UINT Width, Height, MipLevels, ArraySize; int Format; DXGI_SAMPLE_DESC SampleDesc; int Usage; UINT BindFlags, CPUAccessFlags, MiscFlags; };
struct D3D11_SUBRESOURCE_DATA { const void* pSysMem; UINT SysMemPitch, SysMemSlicePitch; };
struct ID3D11Texture2D : IUnknown {};
struct ID3D11ShaderResourceView : IUnknown {};
struct ID3D11Device : IUnknown {
  HRESULT CreateTexture2D(const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D**) { return -1; }
  HRESULT CreateShaderResourceView(ID3D11Texture2D*, const void*, ID3D11ShaderResourceView**) { return -1; } };
