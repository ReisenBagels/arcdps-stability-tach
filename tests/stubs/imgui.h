#pragma once
#include <cstddef>
#include <cstdint>
#include <cfloat>
typedef unsigned int ImU32; typedef void* ImTextureID; typedef int ImGuiCond; typedef int ImGuiWindowFlags; typedef int ImGuiColorEditFlags;
#define IMGUI_VERSION_NUM 18000
#define IM_COL32(r,g,b,a) ((ImU32)(((a)<<24)|((b)<<16)|((g)<<8)|(r)))
struct ImVec2{float x,y;ImVec2():x(0),y(0){}ImVec2(float a,float b):x(a),y(b){}};
struct ImVec4{float x,y,z,w;ImVec4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){}};
struct ImFont{ImVec2 CalcTextSizeA(float,float,float,const char*)const{return ImVec2();}};
struct ImDrawVert{ImU32 col;};
template<class T> struct ImVector{int Size=0;T* Data=nullptr;T& operator[](int i){return Data[i];}};
#define IM_COL32_A_SHIFT 24
#define IM_COL32_A_MASK 0xFF000000
struct ImDrawList{
 ImVector<ImDrawVert> VtxBuffer;
 void AddCircleFilled(ImVec2,float,ImU32,int=0){} void AddCircle(ImVec2,float,ImU32,int=0,float=1){}
 void PathArcTo(ImVec2,float,float,float,int=0){} void PathStroke(ImU32,bool,float){}
 void AddImage(ImTextureID,ImVec2,ImVec2,ImVec2,ImVec2,ImU32){} void AddLine(ImVec2,ImVec2,ImU32,float=1){}
 void AddConvexPolyFilled(const ImVec2*,int,ImU32){} void AddTriangleFilled(ImVec2,ImVec2,ImVec2,ImU32){} void AddRectFilled(ImVec2,ImVec2,ImU32,float=0,int=0){}
 void AddRect(ImVec2,ImVec2,ImU32,float=0,int=0,float=1){} void PathFillConvex(ImU32){} void AddText(ImFont*,float,ImVec2,ImU32,const char*){}};
struct ImGuiContext; struct ImGuiIO{float DeltaTime;}; struct ImGuiSizeCallbackData{ImVec2 DesiredSize; void* UserData;};
typedef int ImGuiTreeNodeFlags;
enum {ImGuiTreeNodeFlags_DefaultOpen=32,ImGuiCond_Always=1,ImGuiWindowFlags_NoMove=256,ImGuiWindowFlags_NoResize=512,ImGuiWindowFlags_NoMouseInputs=1024,ImGuiCond_FirstUseEver=4,ImGuiWindowFlags_NoTitleBar=1,ImGuiWindowFlags_NoScrollbar=2,ImGuiWindowFlags_NoScrollWithMouse=4,ImGuiWindowFlags_NoBackground=8,ImGuiWindowFlags_NoCollapse=16,ImGuiWindowFlags_NoFocusOnAppearing=32,ImGuiWindowFlags_NoNav=64,ImGuiWindowFlags_NoSavedSettings=128,ImGuiColorEditFlags_NoInputs=1};
namespace ImGui{
 inline bool Selectable(const char*,bool=false){return false;} inline bool InputText(const char*,char*,size_t){return false;}
 inline void TextColored(ImVec4,const char*,...){} inline void SetClipboardText(const char*){} inline const char* GetClipboardText(){return nullptr;}
 inline bool IsWindowHovered(){return false;} inline bool IsMouseClicked(int){return false;} inline void SameLine(){}
 inline bool BeginCombo(const char*,const char*){return false;} inline void EndCombo(){}
 inline ImU32 ColorConvertFloat4ToU32(ImVec4){return 0;} inline void ColorConvertHSVtoRGB(float,float,float,float&,float&,float&){}
 inline ImFont* GetFont(){return nullptr;} inline void SetCurrentContext(ImGuiContext*){} inline void SetAllocatorFunctions(void*(*)(size_t,void*),void(*)(void*,void*)){}
 inline double GetTime(){return 0;} inline ImGuiIO& GetIO(){static ImGuiIO i;return i;} inline bool IsMouseDown(int){return false;}
 inline void TextUnformatted(const char*){} inline void TextDisabled(const char*,...){} inline bool Button(const char*){return false;}
 inline bool Checkbox(const char*,bool*){return false;} inline bool Combo(const char*,int*,const char* const*,int){return false;}
 inline bool CollapsingHeader(const char*,int=0){return false;} inline bool ColorEdit3(const char*,float*,int=0){return false;}
 inline bool SliderFloat(const char*,float*,float,float,const char* fmt=nullptr){return false;}
 inline void SetNextWindowPos(ImVec2,int=0){} inline void SetNextWindowSize(ImVec2,int=0){} inline void SetNextWindowSizeConstraints(ImVec2,ImVec2,void(*)(ImGuiSizeCallbackData*),void* ud=nullptr){(void)ud;}
 inline bool Begin(const char*,bool*,int){return false;} inline void End(){} inline ImVec2 GetWindowPos(){return ImVec2();} inline ImVec2 GetWindowSize(){return ImVec2();}
 inline ImDrawList* GetWindowDrawList(){return nullptr;} inline bool BeginPopupContextWindow(const char*){return false;} inline void EndPopup(){}
}
