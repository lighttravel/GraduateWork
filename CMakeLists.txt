# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# PlatformIO sets EXTRA_COMPONENT_DIRS to src. Append local components so
# vendored IDF components (e.g. esp_websocket_client) are discoverable.
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/components")
set(PROJECT_VER "1")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(aichatesp32)
