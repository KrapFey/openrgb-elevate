cmake_minimum_required(VERSION 3.21)
project(OpenRGBElevatePlugin VERSION 1.0.0 LANGUAGES CXX)

# Must match the Qt OpenRGB ships with (OpenRGB 1.1 = Qt 6.8.3, MSVC x64), or OpenRGB rejects the plugin.
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=<qt>/6.8.3/msvc2022_64 -DOPENRGB_SRC=<OpenRGB source at the 1.1 commit>
set(OPENRGB_SRC "" CACHE PATH "OpenRGB source tree (headers only) matching the target OpenRGB version")
set(PLUGIN_URL  "" CACHE STRING "Project URL shown in OpenRGB's plugin list")
if(NOT OPENRGB_SRC)
    message(FATAL_ERROR "Set -DOPENRGB_SRC=<path to OpenRGB source>")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE PLUGIN_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
configure_file(ElevatePlugin.json.in ${CMAKE_BINARY_DIR}/ElevatePlugin.json @ONLY)  # metadata OpenRGB reads before loading

add_library(OpenRGBElevatePlugin SHARED ElevatePlugin.cpp ElevatePlugin.h Elevate.cpp Elevate.h resources.qrc)
target_include_directories(OpenRGBElevatePlugin PRIVATE ${CMAKE_BINARY_DIR}
    ${OPENRGB_SRC} ${OPENRGB_SRC}/RGBController ${OPENRGB_SRC}/dependencies/json)
target_link_libraries(OpenRGBElevatePlugin PRIVATE Qt6::Widgets shell32)
target_compile_definitions(OpenRGBElevatePlugin PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
    PLUGIN_VERSION="${PROJECT_VERSION}" PLUGIN_COMMIT="${PLUGIN_COMMIT}" PLUGIN_URL="${PLUGIN_URL}")

# Release zip: cmake --build build --config Release --target package
install(TARGETS OpenRGBElevatePlugin RUNTIME DESTINATION plugins)
install(FILES README.md LICENSE install.ps1 DESTINATION .)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_FILE_NAME "openrgb-elevate-${PROJECT_VERSION}-openrgb-1.1-win64")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
