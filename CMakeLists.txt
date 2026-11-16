cmake_minimum_required(VERSION 3.22)
project(LaPautaTune VERSION 1.0.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Descarga JUCE solo si no existe ---
include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LaPautaTune
    COMPANY_NAME "La Pauta"
    BUNDLE_ID "com.lapauta.lapautatune"
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE "Lapt"
    PLUGIN_CODE "Lptn"
    FORMATS VST3 Standalone
    PRODUCT_NAME "La Pauta Tune"
)

# --- ESTA LÍNEA GENERA EL JuceHeader.h AUTOMÁTICAMENTE ---
juce_generate_juce_header(LaPautaTune)

target_sources(LaPautaTune PRIVATE
    Source/Engine.cpp
    Source/Engine.h
    Source/Scales.h
    Source/PianoRoll.cpp
    Source/PianoRoll.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(LaPautaTune PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LaPautaTune PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_events
    juce::juce_core
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
