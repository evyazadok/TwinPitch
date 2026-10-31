cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) on macOS
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(TwinPitch VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if you have one (-DJUCE_DIR=/path/to/JUCE), otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(TP_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    set(TP_FORMATS VST3 Standalone)
endif()

juce_add_plugin(TwinPitch
    COMPANY_NAME "Dror Audio"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.droraudio.twinpitch"
    PLUGIN_MANUFACTURER_CODE Drau
    PLUGIN_CODE Twnp
    FORMATS ${TP_FORMATS}
    PRODUCT_NAME "TwinPitch"
    DESCRIPTION "Stereo micro-pitch widener"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Spatial
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(TwinPitch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# Space Mono (SIL Open Font License) is embedded when the font files are present (the workflows download them).
set(TP_FONTS
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Regular.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Bold.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Italic.ttf)
set(TP_HAS_FONT 1)
foreach(f ${TP_FONTS})
    if(NOT EXISTS ${f})
        set(TP_HAS_FONT 0)
    endif()
endforeach()
if(TP_HAS_FONT)
    juce_add_binary_data(TwinPitchData SOURCES ${TP_FONTS})
    target_link_libraries(TwinPitch PRIVATE TwinPitchData)
endif()

target_compile_definitions(TwinPitch PUBLIC
    TP_HAS_FONT=${TP_HAS_FONT}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TwinPitch
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
