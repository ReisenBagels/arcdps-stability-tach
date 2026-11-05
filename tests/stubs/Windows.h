#pragma once
#include <cstdint>
typedef void* HANDLE; typedef void* HWND; typedef unsigned UINT;
extern uint64_t g_fake_now; inline uint64_t GetTickCount64(){return g_fake_now;}
#define __declspec(x)
