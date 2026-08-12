cmake_minimum_required(VERSION 3.22)
project(PalozebraVinyl VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Preferred: put a JUCE checkout in ./JUCE (tested target: JUCE 8.0.8).
# Fallback: CMake will fetch that tag automatically when internet access exists.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PalozebraVinyl
    COMPANY_NAME "Palozebra"
    BUNDLE_ID "com.palozebra.vinyl"
    PLUGIN_MANUFACTURER_CODE PlZb
    PLUGIN_CODE Pvnl
    FORMATS VST3 Standalone
    PRODUCT_NAME "Palozebra Vinyl"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(PalozebraVinyl)

target_sources(PalozebraVinyl PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ScratchEngine.h
)

target_compile_definitions(PalozebraVinyl PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(PalozebraVinyl PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Small JUCE-free smoke test for the scratch DSP core.
add_executable(scratch_engine_test Tests/ScratchEngineTest.cpp)
target_include_directories(scratch_engine_test PRIVATE Source)
