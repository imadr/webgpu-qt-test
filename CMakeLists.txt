cmake_minimum_required(VERSION 2.8)
set(PROJECT "webgpu-qt-test")
project(${PROJECT})

find_package(Qt5 COMPONENTS Widgets Qml Quick OpenGL REQUIRED)

include_directories(${Qt5Widgets_INCLUDE_DIRS} ${QtQml_INCLUDE_DIRS})
add_definitions(${Qt5Widgets_DEFINITIONS} ${QtQml_DEFINITIONS} ${${Qt5Quick_DEFINITIONS}})

qt5_add_resources(QT_RESOURCES qml.qrc)
add_subdirectory("dawn" EXCLUDE_FROM_ALL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT} main.cpp ${QT_RESOURCES})

target_link_libraries(${PROJECT} Qt5::Widgets Qt5::Qml Qt5::Quick Qt5::OpenGL webgpu_cpp webgpu_dawn)