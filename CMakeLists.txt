cmake_minimum_required(VERSION 3.20)
project(BWF VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AE_SDK_ROOT "C:/Users/zp122/Documents/trae_projects/ceshi/AfterEffectsSDK_25.6_61_win/ae25.6_61.64bit.AfterEffectsSDK" CACHE PATH "After Effects SDK root (ae25.6_61.64bit.AfterEffectsSDK)")

set(SOURCES
    BWF.cpp
    BWF_Strings.cpp
    ${AE_SDK_ROOT}/Examples/Util/AEGP_SuiteHandler.cpp
    ${AE_SDK_ROOT}/Examples/Util/MissingSuiteError.cpp
)

include_directories(
    ${AE_SDK_ROOT}/Examples/Headers
    ${AE_SDK_ROOT}/Examples/Headers/SP
    ${AE_SDK_ROOT}/Examples/Headers/Win
    ${AE_SDK_ROOT}/Examples/Resources
    ${AE_SDK_ROOT}/Examples/Util
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_definitions(-DMSWindows -DWIN32 -D_WINDOWS -DAE_OS_WIN -DAE_PROC_INTELx64)

add_library(BWF SHARED ${SOURCES})
set_target_properties(BWF PROPERTIES SUFFIX ".aex" PREFIX "")
target_compile_options(BWF PRIVATE -Wall -O2 -m64 -static-libgcc -static-libstdc++ -finput-charset=UTF-8 -fexec-charset=GBK)
target_link_options(BWF PRIVATE -static-libgcc -static-libstdc++)

set(WINPTHREAD_LIB "C:/mingw64/x86_64-w64-mingw32/lib/libwinpthread.a")
if(EXISTS ${WINPTHREAD_LIB})
    target_link_options(BWF PRIVATE -Wl,--whole-archive ${WINPTHREAD_LIB} -Wl,--no-whole-archive)
endif()

target_link_libraries(BWF PRIVATE kernel32 user32)
