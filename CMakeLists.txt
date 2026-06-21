cmake_minimum_required(VERSION 3.22)

project(AuClear VERSION 0.1.1 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Phase 0–3: Passthrough + DSP rack + real-time AI inference.
# See docs/07-roadmap.md.
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

# --- ONNX Runtime -----------------------------------------------------------
# Phase 3: real-time AI inference.
# Set AUCLEAR_ORT_ROOT to a pre-extracted ORT release directory to skip download.
# e.g. cmake -DAUCLEAR_ORT_ROOT=/path/to/onnxruntime-linux-x64-1.20.0 ..
# Set AUCLEAR_NO_ONNX=ON to build without AI support (classic DSP only).
option(AUCLEAR_NO_ONNX "Build without ONNX Runtime (no AI modules)" OFF)
set(AUCLEAR_ORT_VERSION "1.20.0" CACHE STRING "ONNX Runtime version to fetch")

if(NOT AUCLEAR_NO_ONNX)
    include(FetchContent)

    if(AUCLEAR_ORT_ROOT)
        set(ort_SOURCE_DIR "${AUCLEAR_ORT_ROOT}")
        set(ort_POPULATED TRUE)
    else()
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set(_ORT_ARCHIVE "onnxruntime-linux-x64-${AUCLEAR_ORT_VERSION}.tgz")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(_ORT_ARCHIVE "onnxruntime-osx-universal2-${AUCLEAR_ORT_VERSION}.tgz")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set(_ORT_ARCHIVE "onnxruntime-win-x64-${AUCLEAR_ORT_VERSION}.zip")
        else()
            message(WARNING "AuClear: no pre-built ORT for this platform; set AUCLEAR_NO_ONNX=ON")
            set(AUCLEAR_NO_ONNX ON)
        endif()
    endif()

    if(NOT AUCLEAR_NO_ONNX AND NOT DEFINED ort_SOURCE_DIR)
        FetchContent_Declare(
            ort
            URL "https://github.com/microsoft/onnxruntime/releases/download/v${AUCLEAR_ORT_VERSION}/${_ORT_ARCHIVE}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_GetProperties(ort)
        if(NOT ort_POPULATED)
            FetchContent_Populate(ort)
        endif()
    endif()

    # On Linux, FetchContent may not preserve tgz symlinks (cmake -E tar behaviour
    # varies). Create the SONAME symlink at configure time so the linker can resolve
    # libonnxruntime.so.1 from -L<ort>/lib during the LTO link step.
    if(UNIX AND NOT APPLE AND NOT AUCLEAR_NO_ONNX AND DEFINED ort_SOURCE_DIR)
        set(_ORT_SONAME_LINK "${ort_SOURCE_DIR}/lib/libonnxruntime.so.1")
        if(NOT EXISTS "${_ORT_SONAME_LINK}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "libonnxruntime.so.${AUCLEAR_ORT_VERSION}"
                    "${_ORT_SONAME_LINK}")
        endif()
    endif()

    if(NOT AUCLEAR_NO_ONNX)
        # Find the actual .so/.dylib/.dll inside the extracted directory
        if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set(_ORT_LIB "${ort_SOURCE_DIR}/lib/onnxruntime.lib")
            set(_ORT_DLL "${ort_SOURCE_DIR}/lib/onnxruntime.dll")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(_ORT_LIB "${ort_SOURCE_DIR}/lib/libonnxruntime.${AUCLEAR_ORT_VERSION}.dylib")
        else()
            set(_ORT_LIB "${ort_SOURCE_DIR}/lib/libonnxruntime.so.${AUCLEAR_ORT_VERSION}")
        endif()

        if(EXISTS "${_ORT_LIB}")
            add_library(ort_imported SHARED IMPORTED GLOBAL)
            if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
                set_target_properties(ort_imported PROPERTIES
                    IMPORTED_LOCATION             "${_ORT_DLL}"
                    IMPORTED_IMPLIB               "${_ORT_LIB}"
                    INTERFACE_INCLUDE_DIRECTORIES "${ort_SOURCE_DIR}/include"
                    INTERFACE_COMPILE_DEFINITIONS "AUCLEAR_HAS_ONNXRUNTIME=1")
            else()
                set_target_properties(ort_imported PROPERTIES
                    IMPORTED_LOCATION             "${_ORT_LIB}"
                    INTERFACE_INCLUDE_DIRECTORIES "${ort_SOURCE_DIR}/include"
                    INTERFACE_LINK_DIRECTORIES    "${ort_SOURCE_DIR}/lib"
                    INTERFACE_COMPILE_DEFINITIONS "AUCLEAR_HAS_ONNXRUNTIME=1")
            endif()
            set(AUCLEAR_ORT_TARGET ort_imported)
            message(STATUS "AuClear: ONNX Runtime ${AUCLEAR_ORT_VERSION} found at ${_ORT_LIB}")
        else()
            message(WARNING "AuClear: ORT library not found at ${_ORT_LIB}; AI disabled")
            set(AUCLEAR_NO_ONNX ON)
        endif()
    endif()
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
# Default format set per platform; override with -DAUCLEAR_FORMATS="VST3;LV2;Standalone"
set(AUCLEAR_FORMATS "" CACHE STRING
    "Semicolon-separated plugin formats to build (empty = platform default)")
if(NOT AUCLEAR_FORMATS)
    if(APPLE)
        set(AUCLEAR_FORMATS "AU;Standalone")
    elseif(WIN32)
        set(AUCLEAR_FORMATS "VST3;Standalone")
    else()
        set(AUCLEAR_FORMATS "LV2;Standalone")
    endif()
endif()
message(STATUS "AuClear: building formats — ${AUCLEAR_FORMATS}")

juce_add_plugin(AuClear
    COMPANY_NAME            "AuClear"
    BUNDLE_ID               "com.auclear.auclear"
    PLUGIN_MANUFACTURER_CODE Aucl          # 4 chars, exactly one uppercase
    PLUGIN_CODE             Aucr           # 4 chars, exactly one uppercase
    FORMATS                 ${AUCLEAR_FORMATS}
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
    src/PluginEditor.cpp
    # Engine
    src/engine/ProcessorRack.cpp
    src/engine/ModuleFactory.cpp
    # DSP modules
    src/modules/GainModule.cpp
    src/modules/ParametricEQModule.cpp
    src/modules/GateModule.cpp
    src/modules/CompressorModule.cpp
    src/modules/LimiterModule.cpp
    src/modules/ReverbModule.cpp
    src/modules/DelayModule.cpp
    src/modules/SaturatorModule.cpp
    src/modules/UtilityModule.cpp
    # AI modules (Phase 3)
    src/ai/OnnxSession.cpp
    src/ai/AIEngine.cpp
    src/modules/DenoiseModule.cpp
    src/modules/HumRemoverModule.cpp
    # Real-time stem separation (Phase 4c)
    src/engine/RealtimeStemProcessor.cpp
    # Phase 5 — Dynamic processing
    src/modules/DeEsserModule.cpp
    src/modules/DynamicEQModule.cpp
    # Offline job manager + Demucs runner (Phase 4)
    src/offline/ArtifactCache.cpp
    src/offline/OfflineJobManager.cpp
    src/offline/DemucsSession.cpp
    src/offline/DemucsRunner.cpp
    # File I/O (standalone only)
    src/io/FileProcessor.cpp
    # UI
    src/ui/HeaderComponent.cpp
    src/ui/RackModuleCard.cpp
    src/ui/RackColumn.cpp
    src/ui/MainStage.cpp
    src/ui/MeterBridge.cpp)

target_compile_definitions(AuClear PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0          # requires JUCE licence — see docs/06
    JUCE_REPORT_APP_USAGE=0
    $<$<NOT:$<BOOL:${AUCLEAR_NO_ONNX}>>:AUCLEAR_HAS_ONNXRUNTIME=1>)

target_link_libraries(AuClear
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        $<$<NOT:$<BOOL:${AUCLEAR_NO_ONNX}>>:${AUCLEAR_ORT_TARGET}>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Copy ORT shared library next to each plugin format artefact so it is found at runtime.
if(NOT AUCLEAR_NO_ONNX AND AUCLEAR_ORT_TARGET)
    # $ORIGIN / @loader_path makes the binary look next to itself before the system paths.
    if(APPLE)
        set(_ORT_RPATH "@loader_path")
    elseif(UNIX)
        set(_ORT_RPATH "$ORIGIN")
    endif()

    foreach(_fmt VST3 AU LV2 Standalone)
        if(TARGET AuClear_${_fmt})
            # ORT is PRIVATE to AuClear, so it never appears in format targets' own
            # link commands. With GCC LTO on shared modules the linker requires an
            # explicit reference — add ORT directly to each format target.
            target_link_libraries(AuClear_${_fmt} PRIVATE ${AUCLEAR_ORT_TARGET})

            if(NOT WIN32)
                set_target_properties(AuClear_${_fmt} PROPERTIES
                    BUILD_WITH_INSTALL_RPATH TRUE
                    INSTALL_RPATH            "${_ORT_RPATH}")
            endif()

            if(UNIX AND NOT APPLE)
                target_link_directories(AuClear_${_fmt} PRIVATE "${ort_SOURCE_DIR}/lib")
            endif()

            # Copy the versioned shared library / DLL.
            if(WIN32)
                add_custom_command(TARGET AuClear_${_fmt} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_ORT_DLL}"
                        "$<TARGET_FILE_DIR:AuClear_${_fmt}>"
                    COMMENT "Copying ORT DLL for ${_fmt}")
            else()
                add_custom_command(TARGET AuClear_${_fmt} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_ORT_LIB}"
                        "$<TARGET_FILE_DIR:AuClear_${_fmt}>"
                    COMMENT "Copying ORT library for ${_fmt}")
            endif()

            # Linux: also copy under the SONAME name so dlopen finds libonnxruntime.so.1
            # next to the plugin at runtime (avoids relying on symlinks in the output dir).
            if(UNIX AND NOT APPLE)
                add_custom_command(TARGET AuClear_${_fmt} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_ORT_LIB}"
                        "$<TARGET_FILE_DIR:AuClear_${_fmt}>/libonnxruntime.so.1"
                    COMMENT "Copying ORT SONAME for ${_fmt}")
            endif()
        endif()
    endforeach()
endif()
