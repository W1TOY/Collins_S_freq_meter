# Build wrapper so the ESP32 firmware can be built, flashed and monitored from the
# Collins_S_freq_meter folder. All firmware sources and configuration stay in fmaster/.
cmake_minimum_required(VERSION 3.16)

set(FIRMWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/fmaster")

# fmaster/main is the app component (there is no main/ next to this file)
set(EXTRA_COMPONENT_DIRS "${FIRMWARE_DIR}/main")

# use fmaster/sdkconfig instead of creating a new one here
set(SDKCONFIG "${FIRMWARE_DIR}/sdkconfig")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# keep the component manager's lock file in fmaster/ as well
idf_build_set_property(DEPENDENCIES_LOCK "${FIRMWARE_DIR}/dependencies.lock")

project(fcmaster)
