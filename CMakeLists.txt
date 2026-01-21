cmake_minimum_required(VERSION 3.16)
project(joymanager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
include(FetchContent)

# SimpleBLE
FetchContent_Declare(
  SimpleBLE
  GIT_REPOSITORY https://github.com/OpenBluetoothToolbox/SimpleBLE.git
  GIT_TAG        v0.9.1
)
FetchContent_GetProperties(SimpleBLE)
if(NOT SimpleBLE_POPULATED)
  FetchContent_Populate(SimpleBLE)
  # SimpleBLE root has no CMakeLists.txt, so we add the simpleble subdirectory
  add_subdirectory(${simpleble_SOURCE_DIR}/simpleble ${simpleble_BINARY_DIR})
endif()

# Qt 6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# Project Sources
set(SOURCES
    src/main.cpp
    src/gui/FileManagerView.cpp
    src/gui/DeviceSelectionDialog.cpp
    src/gui/RemoteFileSystemModel.cpp
    src/ble/BleManager.cpp
    src/protocol/PixlProtocol.cpp
)

add_executable(joymanager WIN32 MACOSX_BUNDLE ${SOURCES})

target_include_directories(joymanager PRIVATE
    src
    src/gui
    src/ble
    src/protocol
)

target_link_libraries(joymanager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    simpleble
)

# Qt Automatic Moc/Uic/Rcc
set_property(TARGET joymanager PROPERTY AUTOMOC ON)
set_property(TARGET joymanager PROPERTY AUTOUIC ON)
set_property(TARGET joymanager PROPERTY AUTORCC ON)
