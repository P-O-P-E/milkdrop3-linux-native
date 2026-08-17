cmake_minimum_required(VERSION 3.21)

project(milkdrop3-linux-native
    VERSION 0.2.0
    DESCRIPTION "Native MilkDrop-compatible visualizer for Debian and Ubuntu"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CTest)

option(MILKDROP3_BUILD_APP "Build the SDL/OpenGL visualizer" ON)
option(MILKDROP3_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

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
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imgui_SOURCE_DIR}/misc/cpp
    )
    target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)
    set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_executable(milkdrop3-linux
        src/main.cpp
        src/Application.cpp
        src/AudioCapture.cpp
        src/ProjectMEngine.cpp
        src/UiController.cpp
    )

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

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(milkdrop3-linux PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            $<$<BOOL:${MILKDROP3_WARNINGS_AS_ERRORS}>:-Werror>
        )
    endif()

    install(TARGETS milkdrop3-linux
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
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
    install(FILES ${imgui_SOURCE_DIR}/LICENSE.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/milkdrop3-linux-native
        RENAME LICENSE-imgui.txt
    )

    set(CPACK_PACKAGE_NAME "milkdrop3-linux-native")
    set(CPACK_PACKAGE_VENDOR "P-O-P-E")
    set(CPACK_PACKAGE_CONTACT "https://github.com/P-O-P-E/milkdrop3-linux-native/issues")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native MilkDrop-compatible audio visualizer")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libsdl2-image-2.0-0, libgl1")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    include(CPack)
endif()

if(BUILD_TESTING)
    add_executable(milkdrop3-core-tests tests/core_tests.cpp)
    target_link_libraries(milkdrop3-core-tests PRIVATE milkdrop3_core)
    target_compile_features(milkdrop3-core-tests PRIVATE cxx_std_20)
    add_test(NAME milkdrop3-core-tests COMMAND milkdrop3-core-tests)
endif()
