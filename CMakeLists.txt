cmake_minimum_required(VERSION 3.20)

project(ScreenDimmer)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(LayerShellQt REQUIRED)

qt_standard_project_setup()

add_executable(screen-dimmer
    src/main.cpp
)

target_link_libraries(screen-dimmer PRIVATE
    Qt6::Widgets
    LayerShellQt::Interface
)