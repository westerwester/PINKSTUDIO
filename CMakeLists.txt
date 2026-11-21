cmake_minimum_required(VERSION 3.22)

project(PINKSTUDIO VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Static MSVC runtime: PINKSTUDIO.exe runs without installing the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PINKSTUDIO_BUILD_APP     "Build the PINKSTUDIO desktop application (needs JUCE)" ON)
option(PINKSTUDIO_BUILD_TESTS   "Build the engine unit tests" ON)
option(PINKSTUDIO_BUILD_TOOLS   "Build command-line tools (pinkrender)" ON)
option(PINKSTUDIO_ENABLE_ASIO   "Enable ASIO drivers on Windows (uses the ASIO headers bundled with JUCE 8)" ON)
option(PINKSTUDIO_SANITIZE      "Build engine and tests with Address/UB sanitizers (GCC/Clang)" OFF)
option(PINKSTUDIO_TSAN          "Build engine and tests with ThreadSanitizer (GCC/Clang)" OFF)
set(PINKSTUDIO_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout. Empty = download JUCE automatically.")
set(PINKSTUDIO_JUCE_TAG "8.0.15" CACHE STRING "JUCE git tag used when downloading")

if(PINKSTUDIO_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
if(PINKSTUDIO_TSAN AND NOT MSVC)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

add_subdirectory(engine)

if(PINKSTUDIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PINKSTUDIO_BUILD_TOOLS)
    add_subdirectory(tools/pinkrender)
endif()

if(PINKSTUDIO_BUILD_APP)
    add_subdirectory(app)
endif()
