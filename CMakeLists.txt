cmake_minimum_required(VERSION 3.22)

project(DanceThingy VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files contain UTF-8 Chinese literals; MSVC must be told so.
if (MSVC)
  add_compile_options (/utf-8)
endif()

# ---- JUCE 8 via FetchContent ----
include(FetchContent)

FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.4
  GIT_SHALLOW    TRUE
)

set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JUCE)

# ---- Shared animation code (header-only) ----
add_library(dance_core INTERFACE)
target_include_directories(dance_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Shared)
target_link_libraries(dance_core INTERFACE juce::juce_graphics juce::juce_gui_basics)

# One version string for the overlay's About text, the plugin title and the
# installer: the project version above is the only place it is written down.
target_compile_definitions(dance_core INTERFACE DT_VERSION="${PROJECT_VERSION}")

# ---- Companion VST3: audio passthrough + host transport -> UDP ----
juce_add_plugin(DanceThingy
  COMPANY_NAME "MusicalThingy"
  BUNDLE_ID "com.musicalthingy.dancethingy"
  FORMATS VST3
  PRODUCT_NAME "DanceThingy"
  COPY_PLUGIN_AFTER_BUILD FALSE
  IS_SYNTH FALSE
  IS_MIDI_EFFECT FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE)

target_sources(DanceThingy PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_link_libraries(DanceThingy PRIVATE
  dance_core
  juce::juce_audio_utils
  juce::juce_gui_extra)

# ---- Overlay: transparent always-on-top desktop dancer ----
juce_add_gui_app(DanceOverlay
  COMPANY_NAME "MusicalThingy"
  BUNDLE_ID "com.musicalthingy.danceoverlay"
  PRODUCT_NAME "DanceOverlay")

target_sources(DanceOverlay PRIVATE
  Overlay/OverlayMain.cpp
  Overlay/OverlaySession.cpp
  Overlay/DancerWindow.cpp
  Overlay/SettingsWindow.cpp
  Overlay/LoopbackListener.cpp
  Overlay/WasapiLoopbackCapture.cpp)

target_link_libraries(DanceOverlay PRIVATE
  dance_core
  juce::juce_audio_utils
  juce::juce_gui_extra
  juce::juce_data_structures)

# WASAPI loopback talks to MMDeviceEnumerator / IAudioClient directly.
if (WIN32)
  target_link_libraries(DanceOverlay PRIVATE ole32)
endif()
