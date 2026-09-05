cmake_minimum_required(VERSION 3.22)

project(LUmacOSveler VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
    # JUCE 8.0.8 emits these diagnostics from its module sources when
    # built with newer AppleClang versions.
    add_compile_options(
        -Wno-deprecated-declarations
        -Wno-implicit-int-conversion
        -Wno-switch-enum
    )
endif()

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LUmacOSveler
    COMPANY_NAME "LUmacOSveler"
    BUNDLE_ID com.lumacosveler.plugin
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Lmac
    PLUGIN_CODE Lmvl
    FORMATS VST3 AU
    PRODUCT_NAME "LUmacOSveler"
)

juce_generate_juce_header(LUmacOSveler)

target_sources(LUmacOSveler PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(LUmacOSveler PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LUmacOSveler PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
