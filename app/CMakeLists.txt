# PINKSTUDIO desktop application (JUCE 8). On Windows this produces PINKSTUDIO.exe.

include(FetchContent)

if(PINKSTUDIO_JUCE_DIR)
    add_subdirectory(${PINKSTUDIO_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${PINKSTUDIO_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_gui_app(PinkStudio
    PRODUCT_NAME "PINKSTUDIO"
    COMPANY_NAME "PINKSTUDIO"
    BUNDLE_ID "com.pinkstudio.daw"
    VERSION ${PROJECT_VERSION}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_512.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_256.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "PINKSTUDIO records audio from your inputs.")

target_sources(PinkStudio PRIVATE
    Source/AudioHost.cpp
    Source/Components.cpp
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/PinkTheme.cpp)

target_compile_definitions(PinkStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PinkStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PinkStudio,JUCE_VERSION>"
    PINKSTUDIO_VERSION="${PROJECT_VERSION}")

if(WIN32)
    target_compile_definitions(PinkStudio PRIVATE JUCE_WASAPI=1 JUCE_DIRECTSOUND=1)
    if(PINKSTUDIO_ENABLE_ASIO)
        # JUCE 8.0.11+ bundles the (GPLv3) ASIO headers. See docs/LICENSES.md before distributing.
        target_compile_definitions(PinkStudio PRIVATE JUCE_ASIO=1)
    endif()
endif()

if(UNIX AND NOT APPLE)
    # Linux is only used for CI compile checks / headless screenshots of the shared UI code.
    option(PINKSTUDIO_LINUX_MINIMAL "Disable optional Linux system libraries (ALSA/JACK/X extensions)" ON)
    if(PINKSTUDIO_LINUX_MINIMAL)
        target_compile_definitions(PinkStudio PRIVATE JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0
                                   JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
    endif()
endif()

target_link_libraries(PinkStudio
    PRIVATE
        pinkstudio_engine
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
