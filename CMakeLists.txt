cmake_minimum_required(VERSION 3.1)
project(Gungnir)

set(CMAKE_CXX_STANDARD 11)


include_directories(src)
include_directories(cxxopts/include)

set(LIBS pcrecpp pthread rt)
set(COMPILE_OPTION -Wall
        -Wno-non-template-friend -Woverloaded-virtual
        -Wcast-qual -Wcast-align)

set(CMAKE_CXX_FLAGS_RELEASE "-O3  -Wl,--no-as-needed ")

file(GLOB SRC_FILES
        "src/*.h"
        "src/*.cc"
        )

add_executable(gungnir ${SRC_FILES} artifact/ServerMain.cc)

target_link_libraries(gungnir ${LIBS})
target_compile_options(gungnir PRIVATE ${COMPILE_OPTION})

add_executable(benchmark ${SRC_FILES} artifact/Benchmark.cc)
target_link_libraries(benchmark  ${LIBS})
target_compile_options(benchmark   PRIVATE ${COMPILE_OPTION})

add_executable(validation ${SRC_FILES} artifact/Validation.cc)
target_link_libraries(validation ${LIBS})
target_compile_options(validation PRIVATE ${COMPILE_OPTION})

file(GLOB TEST_FILES
        "test/*.h"
        "test/*.cc"
        )

add_subdirectory(googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(gungnirTest
        ${SRC_FILES} ${TEST_FILES})

target_link_libraries(gungnirTest gtest gtest_main)
target_link_libraries(gungnirTest ${LIBS})

add_test(
        NAME gungnirTest
        COMMAND gungnirTest
)
