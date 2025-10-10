cmake_minimum_required(VERSION 3.16)
project(fireminipro LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/HexView.cpp
    src/ProcessHandling.cpp
)

set(HEADERS
    src/MainWindow.h
    src/HexView.h
    src/ProcessHandling.h
)

qt_add_executable(${PROJECT_NAME}
    ${SOURCES} ${HEADERS}
    resources/icons.qrc
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE Qt6::Widgets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
)
