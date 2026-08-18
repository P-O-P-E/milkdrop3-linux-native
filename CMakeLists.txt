cmake_minimum_required(VERSION 3.21)

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "macOS target architectures" FORCE)
endif()
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum supported macOS version" FORCE)
endif()

project(milkdrop3-linux-native
    VERSION 0.3.0
    DESCRIPTION "Native MilkDrop-compatible visualizer for Linux and Apple Silicon macOS"
    LANGUAGES CXX
)

if(APPLE)
    enable_language(OBJCXX)
endif()

include(GNUInstallDirs)
include(CTest)

option(MILKDROP3_BUILD_APP "Build the SDL/OpenGL visualizer" ON)
option(MILKDROP3_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(MILKDROP3_PROJECTM_LICENSE "" CACHE FILEPATH "Path to the libprojectM license for application bundles")

if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "(^|;)x86_64($|;)")
        message(FATAL_ERROR "The macos-arm64 branch intentionally produces native Apple Silicon binaries only")
    endif()
    if(NOT CMAKE_OSX_ARCHITECTURES MATCHES "(^|;)arm64($|;)")
        message(FATAL_ERROR "Set CMAKE_OSX_ARCHITECTURES=arm64 for the native Apple Silicon build")
    endif()
endif()

add_library(milkdrop3_core
    src/Config.cpp
    src/GeneratedPresetStore.cpp
    src/MashupEngine.cpp
    src/OverlayManager.cpp
    src/PresetCatalog.cpp
    src/PresetDocument.cpp
    src/PresetLibrary.cpp
)

target_include_directories(milkdrop3_core PUBLIC src)
target_compile_features(milkdrop3_core PUBLIC cxx_std_20)
if(APPLE)
    target_compile_definitions(milkdrop3_core PUBLIC MILKDROP3_MACOS_ARM64=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(milkdrop3_core PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        $<$<BOOL:${MILKDROP3_WARNINGS_AS_ERRORS}>:-Werror>
    )
endif()

if(MILKDROP3_BUILD_APP)
    find_package(OpenGL REQUIRED)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(projectM4 4.1 CONFIG REQUIRED)
    if(APPLE)
        # Homebrew's sdl2-compat loads SDL3 with dlopen() instead of linking it,
        # so normal dependency scanning cannot discover the runtime library.
        find_library(MILKDROP3_SDL3_LIBRARY
            NAMES SDL3
            PATHS "/opt/homebrew/opt/sdl3/lib"
            NO_DEFAULT_PATH
            REQUIRED
        )
        find_library(MILKDROP3_SCREENCAPTUREKIT_FRAMEWORK ScreenCaptureKit REQUIRED)
        find_library(MILKDROP3_COREMEDIA_FRAMEWORK CoreMedia REQUIRED)
        find_library(MILKDROP3_FOUNDATION_FRAMEWORK Foundation REQUIRED)
    endif()

    include(FetchContent)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.8
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    # ImGui's public headers use C++11 features such as constexpr. AppleClang
    # otherwise defaults this standalone target to its legacy C++98 mode.
    target_compile_features(imgui PUBLIC cxx_std_11)
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imgui_SOURCE_DIR}/misc/cpp
    )
    target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)
    set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)

    set(MILKDROP3_APP_SOURCES
        src/main.cpp
        src/Application.cpp
        src/AudioCapture.cpp
        src/FadeOverlay.cpp
        src/ProjectMEngine.cpp
        src/UiController.cpp
    )
    if(APPLE)
        find_program(ICONUTIL_EXECUTABLE iconutil REQUIRED)
        set(MILKDROP3_MACOS_ICON "${CMAKE_CURRENT_BINARY_DIR}/milkdrop3-native.icns")
        file(GLOB MILKDROP3_MACOS_ICONSET CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/milkdrop3.iconset/*.png"
        )
        add_custom_command(
            OUTPUT "${MILKDROP3_MACOS_ICON}"
            COMMAND "${ICONUTIL_EXECUTABLE}" -c icns
                    "${CMAKE_CURRENT_SOURCE_DIR}/assets/milkdrop3.iconset"
                    -o "${MILKDROP3_MACOS_ICON}"
            DEPENDS ${MILKDROP3_MACOS_ICONSET}
            COMMENT "Creating the macOS application icon"
            VERBATIM
        )
        set_source_files_properties("${MILKDROP3_MACOS_ICON}" PROPERTIES
            GENERATED TRUE
            MACOSX_PACKAGE_LOCATION Resources
        )
        set(MILKDROP3_MACOS_LICENSES
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/THIRD_PARTY.md"
            "${imgui_SOURCE_DIR}/LICENSE.txt"
        )
        if(MILKDROP3_PROJECTM_LICENSE AND EXISTS "${MILKDROP3_PROJECTM_LICENSE}")
            list(APPEND MILKDROP3_MACOS_LICENSES "${MILKDROP3_PROJECTM_LICENSE}")
        else()
            message(WARNING "Set MILKDROP3_PROJECTM_LICENSE to include the libprojectM license in the app bundle")
        endif()
        set_source_files_properties(${MILKDROP3_MACOS_LICENSES} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/Licenses"
        )
        list(APPEND MILKDROP3_APP_SOURCES
            src/MacSystemAudioCapture.mm
            "${MILKDROP3_MACOS_ICON}"
            ${MILKDROP3_MACOS_LICENSES}
        )
        add_executable(milkdrop3-linux MACOSX_BUNDLE ${MILKDROP3_APP_SOURCES})
    else()
        add_executable(milkdrop3-linux ${MILKDROP3_APP_SOURCES})
    endif()

    target_compile_features(milkdrop3-linux PRIVATE cxx_std_20)
    target_compile_definitions(milkdrop3-linux PRIVATE MILKDROP3_VERSION="${PROJECT_VERSION}")
    target_link_libraries(milkdrop3-linux PRIVATE
        milkdrop3_core
        libprojectM::projectM
        SDL2::SDL2
        SDL2_image::SDL2_image
        imgui
        OpenGL::GL
    )
    if(APPLE)
        if(NOT TARGET SDL2::SDL2main)
            message(FATAL_ERROR "The macOS build requires the SDL2::SDL2main target")
        endif()
        target_link_libraries(milkdrop3-linux PRIVATE SDL2::SDL2main)
        target_link_libraries(milkdrop3-linux PRIVATE
            "${MILKDROP3_SCREENCAPTUREKIT_FRAMEWORK}"
            "${MILKDROP3_COREMEDIA_FRAMEWORK}"
            "${MILKDROP3_FOUNDATION_FRAMEWORK}"
        )
        target_compile_options(milkdrop3-linux PRIVATE
            $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
        )
        set_target_properties(milkdrop3-linux PROPERTIES
            OBJCXX_STANDARD 20
            OBJCXX_STANDARD_REQUIRED YES
            OBJCXX_EXTENSIONS NO
        )
        target_compile_definitions(milkdrop3-linux PRIVATE
            GL_SILENCE_DEPRECATION=1
            MILKDROP3_MACOS_ARM64=1
        )
        set_target_properties(milkdrop3-linux PROPERTIES
            OUTPUT_NAME "MilkDrop3 Native"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "MilkDrop3 Native"
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.p-o-p-e.milkdrop3-native"
            MACOSX_BUNDLE_ICON_FILE "milkdrop3-native.icns"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            BUILD_RPATH "${CMAKE_PREFIX_PATH}"
            INSTALL_RPATH "@executable_path/../Frameworks"
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(milkdrop3-linux PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            $<$<BOOL:${MILKDROP3_WARNINGS_AS_ERRORS}>:-Werror>
        )
    endif()

    if(APPLE)
        install(TARGETS milkdrop3-linux BUNDLE DESTINATION .)

        set(MILKDROP3_BUNDLE_SEARCH_DIRS "/opt/homebrew/lib")
        foreach(prefix IN LISTS CMAKE_PREFIX_PATH)
            list(APPEND MILKDROP3_BUNDLE_SEARCH_DIRS "${prefix}" "${prefix}/lib" "${prefix}/lib64")
        endforeach()
        configure_file(
            cmake/BundleMacOS.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/BundleMacOS.cmake"
            @ONLY
        )
        install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/BundleMacOS.cmake")
    else()
        install(TARGETS milkdrop3-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(FILES packaging/milkdrop3-linux.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        )
        install(FILES assets/milkdrop3-linux.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        )
        install(FILES config/config.ini
            DESTINATION ${CMAKE_INSTALL_DATADIR}/milkdrop3-linux
            RENAME config.example.ini
        )
    endif()
    if(NOT APPLE)
        install(FILES ${imgui_SOURCE_DIR}/LICENSE.txt
            DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/milkdrop3-linux-native
            RENAME LICENSE-imgui.txt
        )
    endif()

    set(CPACK_PACKAGE_NAME "milkdrop3-linux-native")
    set(CPACK_PACKAGE_VENDOR "P-O-P-E")
    set(CPACK_PACKAGE_CONTACT "https://github.com/P-O-P-E/milkdrop3-linux-native/issues")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native MilkDrop-compatible audio visualizer")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    if(APPLE)
        set(CPACK_GENERATOR "DragNDrop;TGZ")
        set(CPACK_PACKAGE_FILE_NAME "milkdrop3-native-${PROJECT_VERSION}-macos-arm64")
        set(CPACK_DMG_VOLUME_NAME "MilkDrop3 Native ${PROJECT_VERSION}")
        set(CPACK_DMG_FORMAT "UDBZ")
    else()
        set(CPACK_GENERATOR "DEB;TGZ")
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
        set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libsdl2-image-2.0-0, libgl1")
    endif()
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    include(CPack)
endif()

if(BUILD_TESTING)
    add_executable(milkdrop3-core-tests tests/core_tests.cpp)
    target_link_libraries(milkdrop3-core-tests PRIVATE milkdrop3_core)
    target_compile_features(milkdrop3-core-tests PRIVATE cxx_std_20)
    add_test(NAME milkdrop3-core-tests COMMAND milkdrop3-core-tests)
endif()
