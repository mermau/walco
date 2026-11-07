cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) su macOS
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(FTG1 VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: usa una copia locale se indicata (-DJUCE_DIR=...), altrimenti la scarica
set(JUCE_DIR "" CACHE PATH "Cartella di JUCE (opzionale)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(FTG1Fonts SOURCES
    resources/fonts/Yellowtail-Regular.ttf
    resources/fonts/ArchivoBlack-Regular.ttf)

option(FTG_BUILD_PLUGIN "Compila il plugin" ON)
if(FTG_BUILD_PLUGIN)
    juce_add_plugin(FTG1
        COMPANY_NAME "Collateral Effects"
        BUNDLE_ID com.collateraleffects.ftg1
        PLUGIN_MANUFACTURER_CODE Clfx
        PLUGIN_CODE Ftg1
        PRODUCT_NAME "FTG-1"
        FORMATS VST3 AU Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE          # riceve i CC dalla tastiera controller
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        AU_MAIN_TYPE kAudioUnitType_Effect   # AU standard ("aufx"): va negli slot Audio FX di Logic
        VST3_CATEGORIES Fx Distortion
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        ICON_BIG resources/icon.png
        MICROPHONE_PERMISSION_ENABLED TRUE        # macOS: senza, la Standalone riceve solo silenzio
        MICROPHONE_PERMISSION_TEXT "FTG-1 usa l'ingresso della scheda audio per elaborare il segnale della chitarra."
        COPY_PLUGIN_AFTER_BUILD TRUE)

    target_sources(FTG1 PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(FTG1 PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(FTG1
        PRIVATE FTG1Fonts juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endif()

# Test del modello DSP (senza JUCE)
option(FTG_BUILD_TESTS "Compila i test del circuito" ON)
if(FTG_BUILD_TESTS)
    add_executable(circuit_test tests/circuit_test.cpp)
    target_include_directories(circuit_test PRIVATE Source)
    add_executable(click_test tests/click_test.cpp)
    target_include_directories(click_test PRIVATE Source)
endif()

# Strumento per generare resources/icon.png e lo screenshot dell'interfaccia
option(FTG_BUILD_SNAPSHOT "Compila lo strumento icona/screenshot" OFF)
if(FTG_BUILD_SNAPSHOT)
    juce_add_console_app(ftg_snapshot PRODUCT_NAME "ftg_snapshot")
    target_sources(ftg_snapshot PRIVATE tools/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(ftg_snapshot PRIVATE JucePlugin_Name="FTG-1"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ftg_snapshot PRIVATE FTG1Fonts juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()

# Test del processore completo (blocchi, CC, bypass, stato): Linux/macOS, opzionale
option(FTG_BUILD_PROCESSOR_TEST "Compila il test del processore" OFF)
if(FTG_BUILD_PROCESSOR_TEST)
    juce_add_console_app(processor_test PRODUCT_NAME "processor_test")
    target_sources(processor_test PRIVATE tools/processor_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(processor_test PRIVATE JucePlugin_Name="FTG-1" JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(processor_test PRIVATE FTG1Fonts juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
