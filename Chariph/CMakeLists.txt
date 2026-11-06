cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(Chariph VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

set(SL_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    set(SL_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Chariph
    COMPANY_NAME "Dror Audio"
    BUNDLE_ID "com.droraudio.chariph"
    PLUGIN_MANUFACTURER_CODE Drau
    PLUGIN_CODE Chrf
    FORMATS ${SL_FORMATS}
    PRODUCT_NAME "CHARIPH"
    DESCRIPTION "Distortion box with 40 styles"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(Chariph PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# Space Mono (SIL Open Font License) is embedded when the font files are present (the workflows download them).
set(SL_FONTS
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Regular.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Bold.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Italic.ttf)
set(CH_HAS_FONT 1)
foreach(f ${SL_FONTS})
    if(NOT EXISTS ${f})
        set(CH_HAS_FONT 0)
    endif()
endforeach()
if(CH_HAS_FONT)
    juce_add_binary_data(ChariphData SOURCES ${SL_FONTS})
    target_link_libraries(Chariph PRIVATE ChariphData)
endif()

target_compile_definitions(Chariph PUBLIC
    CH_HAS_FONT=${CH_HAS_FONT}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Chariph
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
