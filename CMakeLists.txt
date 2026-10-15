cmake_minimum_required(VERSION 3.22)

project(HarmonySingerByLR VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment target")

# ---------------------------------------------------------------------------
# JUCE is fetched automatically at CI/build time (needs internet access).
# This is why the project is built on GitHub Actions rather than offline.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Embed the GUI assets (knobs, buttons, LEDs, background) as BinaryData
# ---------------------------------------------------------------------------
file(GLOB RESOURCE_FILES ${CMAKE_SOURCE_DIR}/Resources/*.png)

juce_add_binary_data(HarmonySingerAssets
    SOURCES
        ${RESOURCE_FILES}
)
set_target_properties(HarmonySingerAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ---------------------------------------------------------------------------
# The plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(HarmonySingerByLR
    COMPANY_NAME                "LR"
    PLUGIN_MANUFACTURER_CODE    LRpl
    PLUGIN_CODE                 HmSg
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "TC HELICON by LR"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx "Pitch Shift"
    ICON_BIG                    "${CMAKE_SOURCE_DIR}/Icons/icon_big.png"
    ICON_SMALL                  "${CMAKE_SOURCE_DIR}/Icons/icon_small.png"
)

juce_generate_juce_header(HarmonySingerByLR)

target_sources(HarmonySingerByLR
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/dsp/YinPitchDetector.cpp
        Source/dsp/GranularPitchShifter.cpp
        Source/dsp/HarmonyEngine.cpp
        Source/dsp/AdaptiveTone.cpp
)

target_compile_definitions(HarmonySingerByLR
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(HarmonySingerByLR
    PRIVATE
        HarmonySingerAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
