cmake_minimum_required(VERSION 3.20)
project(SLMaster)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} /O2 /Zi")

set(VTK_DIR "F:/code/vcpkg/packages/vtk_x64-windows/share/vtk" )
set(Eigen3_DIR "F:/code/vcpkg/packages/eigen3_x64-windows/share/eigen3")
set(Eigen3_INCLUDE_DIR "F:/code/vcpkg/packages/eigen3_x64-windows/include")

set(nlohmann_json_DIR "F:/code/vcpkg/packages/nlohmann-json_x64-windows/share/nlohmann_json")
set(pugixml_DIR "F:/code/vcpkg/packages/pugixml_x64-windows/share/pugixml")
set(double-conversion_DIR "F:/code/vcpkg/packages/double-conversion_x64-windows/share/double-conversion")
set(LZ4_DIR "F:/code/vcpkg/packages/lz4_x64-windows/share/lz4")
set(FastFloat_DIR "F:/code/vcpkg/packages/fast-float_x64-windows/share/FastFloat")
set(Verdict_DIR "F:/code/vcpkg/packages/verdict_x64-windows/share/verdict")


add_subdirectory(src)

option(BUILD_GUI "build the gui." ON)

if(BUILD_GUI)
    add_subdirectory(gui)
    message(NOTICE "[SLMaster]: build gui...")
endif()

option(BUILD_TEST "build the test case with google_test." ON)

if(BUILD_TEST)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
    message(NOTICE "[SLMaster]: build test case...")
endif()

option(BUILD_PERF "build the perfermance case with google benchmark." ON)

if(BUILD_TEST)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/perf)
    message(NOTICE "[SLMaster]: build perfermance case...")
endif()