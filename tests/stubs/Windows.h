#pragma once
#include <cstdint>
typedef void* HANDLE; typedef void* HWND; typedef unsigned UINT;
extern uint64_t g_fake_now; inline uint64_t GetTickCount64(){return g_fake_now;}
#define __declspec(x)

#include <cstdio>
typedef void* HMODULE; typedef const char* LPCSTR; typedef unsigned long DWORD; typedef long HRESULT; typedef long long INT_PTR; typedef unsigned UINT_;
#define MAX_PATH 260
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 4
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 2
#define SUCCEEDED(h) ((h) >= 0)
#define FAILED(h) ((h) < 0)
inline bool GetModuleHandleExA(DWORD, LPCSTR, HMODULE*){return false;}
inline DWORD GetModuleFileNameA(HMODULE, char*, DWORD){return 0;}
inline FILE* _wfopen(const wchar_t*, const wchar_t*){return nullptr;}
#define __uuidof(x) 0
struct IUnknown { virtual HRESULT QueryInterface(int, void**) { return -1; } virtual unsigned long Release() { return 0; } };
