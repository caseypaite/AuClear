cmake_minimum_required(VERSION 3.22)

project(AuClear VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Phase 0 scaffold: empty passthrough plugin that builds to VST3 / AU / LV2 /
# Standalone from a single JUCE codebase. See docs/07-roadmap.md.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Universal binaries on macOS so one build serves Apple Silicon + Intel hosts.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif()

# --- JUCE -------------------------------------------------------------------
# Pulled via FetchContent so the build is reproducible with no global install.
# Override with -DAUCLEAR_JUCE_TAG=... or point JUCE_SOURCE_DIR at a local copy.
include(FetchContent)

set(AUCLEAR_JUCE_TAG "8.0.4" CACHE STRING "JUCE git tag to build against")

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${AUCLEAR_JUCE_TAG}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- Plugin target ----------------------------------------------------------
juce_add_plugin(AuClear
    COMPANY_NAME            "AuClear"
    BUNDLE_ID               "com.auclear.auclear"
    PLUGIN_MANUFACTURER_CODE Aucl          # 4 chars, exactly one uppercase
    PLUGIN_CODE             Aucr           # 4 chars, exactly one uppercase
    FORMATS                 VST3 AU LV2 Standalone
    PRODUCT_NAME            "AuClear"
    DESCRIPTION             "AI audio repair, restoration and stem-remix rack"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES         Fx Restoration
    AU_MAIN_TYPE            kAudioUnitType_Effect
    LV2URI                  "https://auclear.com/plugins/auclear")

juce_generate_juce_header(AuClear)

target_sources(AuClear PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_compile_definitions(AuClear PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0          # requires JUCE licence — see docs/06
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(AuClear
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
