cmake_minimum_required(VERSION 3.20)
project(malid3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(FATAL_ERROR "malid3d builds Windows DLLs. Use one of the toolchain files in cmake/.")
endif()

set(MALID3D_VULKAN_HEADERS "${CMAKE_SOURCE_DIR}/third_party/Vulkan-Headers/include"
    CACHE PATH "Path to the Khronos Vulkan-Headers include directory")
if(NOT EXISTS "${MALID3D_VULKAN_HEADERS}/vulkan/vulkan_core.h")
  message(FATAL_ERROR "Vulkan headers not found at ${MALID3D_VULKAN_HEADERS}")
endif()

set(MALID3D_BUILD "local" CACHE STRING "Build identifier shown in the log")

add_library(d3d11 SHARED
  src/d3d11_main.cpp
  src/log.cpp
  src/vk_probe.cpp
  src/d3d11.def)

set_target_properties(d3d11 PROPERTIES PREFIX "" OUTPUT_NAME "d3d11")
target_include_directories(d3d11 PRIVATE src)
target_include_directories(d3d11 SYSTEM PRIVATE "${MALID3D_VULKAN_HEADERS}")
target_compile_definitions(d3d11 PRIVATE
  WIN32_LEAN_AND_MEAN NOMINMAX "MALID3D_BUILD=\"${MALID3D_BUILD}\"")
target_compile_options(d3d11 PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)

# No runtime DLL dependencies beyond what Wine always provides.
target_link_options(d3d11 PRIVATE -static -static-libgcc -static-libstdc++)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  # 32-bit stdcall names are decorated (_Name@N); export the plain names.
  target_link_options(d3d11 PRIVATE -Wl,--enable-stdcall-fixup -Wl,--kill-at)
endif()
