cmake_minimum_required(VERSION 3.28)
project(CMakeSFMLProject LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "src/*.h")

add_executable(main ${SOURCES} ${HEADERS} "src/TradeStrategy.h" "src/RandomStrategy.h" "src/Trader.cpp" "src/TrendStrategy.cpp" "src/Clock.h" "src/Clock.cpp" "src/RandomStrategy.cpp" "src/CandleChart.h" "src/CandleChart.cpp")

target_link_libraries(main PRIVATE SFML::Graphics)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/fonts" 
     DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")