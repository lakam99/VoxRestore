cmake_minimum_required(VERSION 3.22)
project(VoxRestore VERSION 0.1.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(VOX_VENDOR "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "JUCE and ONNX Runtime parent")
if(NOT EXISTS "${VOX_VENDOR}/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "Use the complete source distribution including vendored third_party dependencies.")
endif()
add_subdirectory("${VOX_VENDOR}/JUCE" "${CMAKE_BINARY_DIR}/JUCE")
juce_add_binary_data(VoxArtwork SOURCES
    resources/silero_vad.onnx resources/generated/ivory.png resources/generated/graphite.png resources/generated/walnut.png
    resources/generated/knob.png resources/generated/meter.png resources/generated/switches.png resources/generated/buttons.png
    resources/generated/lamps.png resources/generated/vfd.png resources/generated/transport.png resources/generated/app-icon.png)
juce_add_plugin(VoxRestore COMPANY_NAME "Arkam Audio" BUNDLE_ID "audio.arkam.voxrestore"
    PLUGIN_MANUFACTURER_CODE Arkm PLUGIN_CODE Vxrs FORMATS VST3 PRODUCT_NAME "VoxRestore"
    ICON_BIG resources/generated/app-icon.png ICON_SMALL resources/generated/app-icon.png
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Restoration NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE)
target_sources(VoxRestore PRIVATE Source/VoxDsp.cpp Source/Reference.cpp Source/VoiceDetector.cpp Source/AsyncCleanup.cpp
    Source/Processor.cpp Source/Editor.cpp Source/RasterAssets.cpp Source/VfdDisplay.cpp Source/Tests.cpp Source/PluginEntry.cpp)
target_include_directories(VoxRestore PUBLIC Source "${VOX_VENDOR}/JUCE/modules" PRIVATE "${VOX_VENDOR}/onnxruntime/build/native/include")
target_compile_definitions(VoxRestore PUBLIC JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0 JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(VoxRestore PRIVATE VoxArtwork juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
juce_add_gui_app(VoxRestoreApp COMPANY_NAME "Arkam Audio" PRODUCT_NAME "VoxRestore" ICON_BIG resources/generated/app-icon.png ICON_SMALL resources/generated/app-icon.png)
target_sources(VoxRestoreApp PRIVATE Source/Standalone.cpp Source/OsTheme.cpp Source/UpdaterLifecycle.cpp)
if(APPLE)
    target_sources(VoxRestoreApp PRIVATE Source/UpdaterMac.cpp Source/MacCapture.mm)
else()
    target_sources(VoxRestoreApp PRIVATE Source/Updater.cpp)
endif()
target_compile_definitions(VoxRestoreApp PRIVATE VOX_VERSION="${PROJECT_VERSION}")
target_link_libraries(VoxRestoreApp PRIVATE VoxRestore juce::juce_recommended_config_flags)
if(WIN32)
    target_link_libraries(VoxRestoreApp PRIVATE dwmapi shell32)
    juce_add_gui_app(VoxRestoreTeardownHost COMPANY_NAME "VoxRestore tests" PRODUCT_NAME "VoxRestoreTeardownHost")
    target_sources(VoxRestoreTeardownHost PRIVATE Source/TeardownHost.cpp)
    target_link_libraries(VoxRestoreTeardownHost PRIVATE VoxRestore juce::juce_recommended_config_flags dwmapi)
    foreach(script VoxRestore-Update.ps1 Install-VoxRestore.ps1 Release-Common.ps1 Uninstall-VoxRestore.ps1)
        set_property(TARGET VoxRestoreApp APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/${script}")
        add_custom_command(TARGET VoxRestoreApp POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/scripts/${script}" "$<TARGET_FILE_DIR:VoxRestoreApp>/${script}")
    endforeach()
    foreach(target VoxRestore_VST3 VoxRestoreApp)
        foreach(dll onnxruntime.dll onnxruntime_providers_shared.dll)
            add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VOX_VENDOR}/onnxruntime/runtimes/win-x64/native/${dll}" "$<TARGET_FILE_DIR:${target}>")
        endforeach()
    endforeach()
elseif(APPLE)
    foreach(target VoxRestore_VST3 VoxRestoreApp)
        add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:${target}>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VOX_VENDOR}/onnxruntime/runtimes/osx-arm64/native/libonnxruntime.dylib" "$<TARGET_BUNDLE_DIR:${target}>/Contents/Frameworks/libonnxruntime.dylib"
            COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:${target}>")
    endforeach()
endif()
