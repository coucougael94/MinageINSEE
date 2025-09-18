cmake_minimum_required(VERSION 3.16)

project(CxW LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Sql)

add_executable(CxW
  main.cpp
  villecritique.h villecritique.cpp
  weather.h weather.cpp
)
target_link_libraries(CxW Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Core)

include(GNUInstallDirs)
install(TARGETS CxW
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
