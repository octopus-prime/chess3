cmake_minimum_required(VERSION 3.28)

project(chess LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

file(GLOB_RECURSE types_modules CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/types/*.cppm"
)
file(GLOB_RECURSE core_modules CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cppm"
)
file(GLOB_RECURSE core_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
)
list(REMOVE_ITEM core_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/core/main.cpp")
file(GLOB_RECURSE engine_modules CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cppm"
)
file(GLOB_RECURSE engine_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp"
)
list(REMOVE_ITEM engine_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/main.cpp")
file(GLOB_RECURSE nnue_modules CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nnue/*.cppm"
)
file(GLOB_RECURSE nnue_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nnue/*.cpp"
)
list(REMOVE_ITEM nnue_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/nnue/main.cpp")

# add_compile_options(-fprofile-generate)
# add_link_options(-fprofile-generate)

# llvm-profdata merge -output=foo *.profraw

# add_compile_options(-fprofile-use=foo)
# add_link_options(-fprofile-use=foo)

add_library(chess_types STATIC)
target_compile_features(chess_types PUBLIC cxx_std_26)
target_compile_options(chess_types PUBLIC -march=native -fconstexpr-steps=500000000 -Wno-c23-extensions)
target_sources(chess_types PUBLIC FILE_SET CXX_MODULES FILES ${types_modules})

add_library(chess_core STATIC)
target_link_libraries(chess_core PUBLIC chess_types)
target_sources(chess_core
    PUBLIC FILE_SET CXX_MODULES FILES ${core_modules}
    PRIVATE ${core_sources}
)

add_library(chess_nnue STATIC)
target_link_libraries(chess_nnue PUBLIC chess_core)
target_sources(chess_nnue
    PUBLIC FILE_SET CXX_MODULES FILES ${nnue_modules}
    PRIVATE ${nnue_sources}
)

add_library(chess_engine STATIC)
target_link_libraries(chess_engine PUBLIC chess_core chess_nnue)
add_dependencies(chess_engine chess_nnue)
target_sources(chess_engine
    PUBLIC FILE_SET CXX_MODULES FILES ${engine_modules}
    PRIVATE ${engine_sources}
)

add_executable(perft src/core/main.cpp)
target_link_libraries(perft PRIVATE chess_core)

add_executable(nnue_demo src/nnue/main.cpp)
target_link_libraries(nnue_demo PRIVATE chess_nnue)

add_executable(demo src/engine/main.cpp)
target_link_libraries(demo PRIVATE chess_engine)

include(CTest)
if(BUILD_TESTING)
    add_executable(chess_types_tests test/types/main.cpp)
    target_include_directories(chess_types_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")
    target_link_libraries(chess_types_tests PRIVATE chess_types)
    add_test(NAME chess.types COMMAND chess_types_tests)

    add_executable(chess_core_tests
        test/core/main.cpp
        test/core/lookup.cpp
        test/core/position.cpp
        test/core/perft.cpp
    )
    target_include_directories(chess_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")
    target_link_libraries(chess_core_tests PRIVATE chess_core)
    foreach(suite IN ITEMS lookup position perft)
        add_test(NAME chess.${suite} COMMAND chess_core_tests ${suite})
        set_tests_properties(chess.${suite} PROPERTIES LABELS core)
    endforeach()
    add_executable(chess_nnue_tests test/nnue/main.cpp test/nnue/nnue.cpp)
    target_include_directories(chess_nnue_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")
    target_link_libraries(chess_nnue_tests PRIVATE chess_nnue)
    add_test(NAME chess.nnue COMMAND chess_nnue_tests)
    set_tests_properties(chess.nnue PROPERTIES LABELS nnue)

    add_executable(chess_engine_tests
        test/engine/main.cpp
        test/engine/negamax.cpp
        test/engine/evaluator.cpp
    )
    target_include_directories(chess_engine_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")
    target_link_libraries(chess_engine_tests PRIVATE chess_engine)
    foreach(suite IN ITEMS negamax evaluator)
        add_test(NAME chess.${suite} COMMAND chess_engine_tests ${suite})
        set_tests_properties(chess.${suite} PROPERTIES LABELS engine)
    endforeach()
    add_test(NAME chess.engine COMMAND chess_engine_tests)
    set_tests_properties(chess.types PROPERTIES LABELS types)
    set_tests_properties(chess.engine PROPERTIES LABELS engine)
endif()
