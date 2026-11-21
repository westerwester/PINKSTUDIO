# pinkstudio_engine: the real-time audio engine. Pure C++20, no JUCE, no GUI.
add_library(pinkstudio_engine STATIC
    src/AudioEngine.cpp
    src/DemoProject.cpp
    src/Meters.cpp
    src/Mixer.cpp
    src/OfflineRenderer.cpp
    src/PinkDrum.cpp
    src/PinkSynth.cpp
    src/Project.cpp
    src/StockEffects.cpp
    src/TempoMap.cpp
    src/UndoManager.cpp
    src/WavFile.cpp
)

target_include_directories(pinkstudio_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(pinkstudio_engine PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(pinkstudio_engine PRIVATE /W4 /permissive- /utf-8 /wd4100)
    target_compile_definitions(pinkstudio_engine PUBLIC NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(pinkstudio_engine PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
