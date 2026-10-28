cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(TwentyFourShimmers VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SHIMMER_BUILD_TESTS "Build the offline DSP tests" ON)
option(SHIMMER_BUILD_SNAPSHOT "Build the UI snapshot tool (development only)" OFF)

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise fetch it.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(TwentyFourShimmers
    PRODUCT_NAME "24 Shimmers"
    COMPANY_NAME "Thiel"
    BUNDLE_ID "com.thiel.twentyfourshimmers"
    DESCRIPTION "Lucha libre shimmer reverb"
    PLUGIN_MANUFACTURER_CODE Thie
    PLUGIN_CODE Sh24
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(TwentyFourShimmers
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LuchaStyle.cpp
        Source/MaskArt.cpp)

target_compile_definitions(TwentyFourShimmers
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TwentyFourShimmers
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(SHIMMER_BUILD_TESTS)
    add_executable(EngineTest tests/EngineTest.cpp)
    enable_testing()
    add_test(NAME EngineTest COMMAND EngineTest)
endif()

if(SHIMMER_BUILD_SNAPSHOT)
    juce_add_console_app(UISnapshot PRODUCT_NAME "UISnapshot")
    target_sources(UISnapshot
        PRIVATE
            tests/UISnapshot.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/LuchaStyle.cpp
            Source/MaskArt.cpp)
    target_compile_definitions(UISnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="24 Shimmers")
    target_link_libraries(UISnapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
