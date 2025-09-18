cmake_minimum_required(VERSION 3.16)
project(WeatherRetriever)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

set(CMAKE_AUTOMOC ON)

set(SOURCES
    main.cpp
    weatherretriever.cpp
)

set(HEADERS
    weatherretriever.h
)

add_executable(WeatherRetriever ${SOURCES} ${HEADERS})

target_link_libraries(WeatherRetriever Qt6::Core Qt6::Network)

# Pour le débogage
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(WeatherRetriever PRIVATE QT_DEBUG)
endif()

# Installation
install(TARGETS WeatherRetriever
    RUNTIME DESTINATION bin
)
