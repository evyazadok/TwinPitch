cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(VoxMorph VERSION 1.0.0)

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

set(VM_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    set(VM_FORMATS VST3 Standalone)
endif()

juce_add_plugin(VoxMorph
    COMPANY_NAME "Dror Audio"
    BUNDLE_ID "com.droraudio.voxmorph"
    PLUGIN_MANUFACTURER_CODE Drau
    PLUGIN_CODE Vxmr
    FORMATS ${VM_FORMATS}
    PRODUCT_NAME "VoxMorph"
    DESCRIPTION "Voice pitch and formant shifter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Pitch Shift
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(VoxMorph PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# Space Mono (SIL Open Font License) is embedded when the font files are present (the workflows download them).
set(VM_FONTS
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Regular.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Bold.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SpaceMono-Italic.ttf)
set(VM_HAS_FONT 1)
foreach(f ${VM_FONTS})
    if(NOT EXISTS ${f})
        set(VM_HAS_FONT 0)
    endif()
endforeach()
if(VM_HAS_FONT)
    juce_add_binary_data(VoxMorphData SOURCES ${VM_FONTS})
    target_link_libraries(VoxMorph PRIVATE VoxMorphData)
endif()

target_compile_definitions(VoxMorph PUBLIC
    VM_HAS_FONT=${VM_HAS_FONT}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VoxMorph
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
