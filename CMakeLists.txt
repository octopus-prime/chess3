cmake_minimum_required(VERSION 3.28)

project(chess LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

file(GLOB_RECURSE chess_modules CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm"
)
file(GLOB_RECURSE chess_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(REMOVE_ITEM chess_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(chess_core STATIC)
target_compile_features(chess_core PUBLIC cxx_std_23)
target_compile_options(chess_core PUBLIC -march=native -fconstexpr-steps=500000000)

target_sources(chess_core
    PUBLIC FILE_SET CXX_MODULES FILES ${chess_modules}
    PRIVATE ${chess_sources}
)

add_executable(chess src/main.cpp)
target_link_libraries(chess PRIVATE chess_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(chess_tests
        tests/test_main.cpp
        tests/lookup_tests.cpp
        tests/position_tests.cpp
        tests/perft_tests.cpp
    )
    target_include_directories(chess_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(chess_tests PRIVATE chess_core)
    foreach(suite IN ITEMS lookup position perft)
        add_test(NAME chess.${suite} COMMAND chess_tests ${suite})
    endforeach()
endif()
