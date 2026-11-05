cmake_minimum_required(VERSION 3.21)
project(arcdps_stability_tach LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ImGui must match the version ArcDPS was built with (IMGUI_VERSION_NUM is compared at load time).
# ArcDPS currently reports 19270 = 1.92.7. If you see "imgui version mismatch", change this tag.
set(ARC_IMGUI_TAG v1.92.7)

# Downloads zip archives (no git needed). To work offline, extract them yourself and pass
#   -DFETCHCONTENT_SOURCE_DIR_IMGUI=<folder> -DFETCHCONTENT_SOURCE_DIR_STB=<folder>
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/${ARC_IMGUI_TAG}.zip)
FetchContent_Declare(stb   URL https://github.com/nothings/stb/archive/refs/heads/master.zip)
FetchContent_MakeAvailable(imgui stb)

add_library(stability_tach SHARED
  src/main.cpp src/tracker.cpp src/gauge.cpp src/skin.cpp src/settings.cpp src/log.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
target_include_directories(stability_tach PRIVATE src ${imgui_SOURCE_DIR} ${stb_SOURCE_DIR})
target_link_libraries(stability_tach PRIVATE d3d11 dxgi)
target_compile_definitions(stability_tach PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
# ArcDPS loads plugins named d3d11_arcdps_*.dll
set_target_properties(stability_tach PROPERTIES PREFIX "" OUTPUT_NAME "d3d11_arcdps_stability_tach")
