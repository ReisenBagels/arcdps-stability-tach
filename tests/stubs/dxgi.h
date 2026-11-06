#pragma once
#include <d3d11.h>
struct IDXGISwapChain : IUnknown { HRESULT GetDevice(int, void**) { return -1; } };
