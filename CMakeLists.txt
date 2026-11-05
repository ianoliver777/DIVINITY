cmake_minimum_required(VERSION 3.22)

project(DIVINITY VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

# ---------------------------------------------------------------------------
# JUCE: use a local ./JUCE checkout if present, otherwise fetch it.
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(DIVINITY_BUILD_TESTS "Build the DIVINITY test runner" ON)

# Formats: AU only on macOS.
set(DIVINITY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DIVINITY_FORMATS AU)
endif()

juce_add_plugin(DIVINITY
    COMPANY_NAME "TRIPLE SEVEN AUDIO"
    COMPANY_WEBSITE "https://www.triple7.world"
    BUNDLE_ID "com.triplesevenaudio.divinity"
    PLUGIN_MANUFACTURER_CODE T7au
    PLUGIN_CODE Dvnt
    FORMATS ${DIVINITY_FORMATS}
    PRODUCT_NAME "DIVINITY"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_add_binary_data(DivinityAssets
    HEADER_NAME DivinityAssets.h
    NAMESPACE DivinityAssets
    SOURCES
        Resources/Fonts/Inter-Regular.ttf
        Resources/Fonts/Inter-Medium.ttf
        Resources/Fonts/Inter-SemiBold.ttf
        Resources/Images/triple_seven_logo.png
        Resources/Images/barbed_frown.png)

set(DIVINITY_CORE_SOURCES
    Source/Parameters.cpp
    Source/PluginProcessor.cpp
    Source/DSP/Wavetables.cpp
    Source/DSP/SynthEngine.cpp
    Source/DSP/FXChain.cpp
    Source/DSP/SoulEngine.cpp
    Source/DSP/FxCatalog.cpp
    Source/DSP/Arpeggiator.cpp
    Source/Presets/PresetManager.cpp
    Source/Presets/FactoryPresets.cpp
    Source/Presets/FactoryPresetsWave.cpp
    Source/PluginEditor.cpp
    Source/UI/DivinityLookAndFeel.cpp
    Source/UI/Components.cpp
    Source/UI/Panels.cpp
    Source/UI/CreateView.cpp
    Source/UI/SoulView.cpp
    Source/UI/Pages.cpp
    Source/UI/GuardianPanel.cpp
    Source/Assistant/GuardianBrain.cpp)

target_sources(DIVINITY PRIVATE ${DIVINITY_CORE_SOURCES})
target_include_directories(DIVINITY PRIVATE Source)

set(DIVINITY_COMMON_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(DIVINITY PUBLIC ${DIVINITY_COMMON_DEFS})

# Release builds keep debug symbols on MSVC so crash stacks are readable (PDBs aren't shipped).
if(MSVC)
    target_compile_options(DIVINITY PUBLIC $<$<CONFIG:Release>:/Zi>)
    target_link_options(DIVINITY PUBLIC $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()

set(DIVINITY_JUCE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra)

target_link_libraries(DIVINITY
    PRIVATE
        DivinityAssets
        ${DIVINITY_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Test runner: compiles the same sources as a console app and exercises DSP,
# MIDI, state round-trips, presets and the editor offscreen.
# ---------------------------------------------------------------------------
if(DIVINITY_BUILD_TESTS)
    juce_add_console_app(DivinityTests PRODUCT_NAME "DivinityTests")
    target_sources(DivinityTests PRIVATE Tests/TestMain.cpp ${DIVINITY_CORE_SOURCES})
    target_include_directories(DivinityTests PRIVATE Source)
    target_compile_definitions(DivinityTests PRIVATE ${DIVINITY_COMMON_DEFS}
        JUCE_STANDALONE_APPLICATION=1 DIVINITY_TESTS=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(DivinityTests PRIVATE
        DivinityAssets ${DIVINITY_JUCE_MODULES}
        juce::juce_recommended_config_flags)

    # Sound Lab: objective audio measurements (aliasing, unison, filter, reverb, presets)
    juce_add_console_app(DivinitySoundLab PRODUCT_NAME "DivinitySoundLab")
    target_sources(DivinitySoundLab PRIVATE Tests/SoundLab.cpp ${DIVINITY_CORE_SOURCES})
    target_include_directories(DivinitySoundLab PRIVATE Source)
    target_compile_definitions(DivinitySoundLab PRIVATE ${DIVINITY_COMMON_DEFS}
        JUCE_STANDALONE_APPLICATION=1 DIVINITY_TESTS=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(DivinitySoundLab PRIVATE
        DivinityAssets ${DIVINITY_JUCE_MODULES}
        juce::juce_recommended_config_flags)
endif()
