cmake_minimum_required(VERSION 3.13)
project(Gungnir)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)

set(LIBS pcrecpp pthread)

file(GLOB SRC_FILES
        "src/*.h"
        "src/*.cc"
        )

add_executable(Gungnir ${SRC_FILES} artifact/ServerMain.cc)

target_link_libraries(Gungnir ${LIBS})

file(GLOB TEST_FILES
        "test/*.h"
        "test/*.cc"
        )

add_subdirectory(googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(GungnirTest
        ${SRC_FILES} ${TEST_FILES})

target_link_libraries(GungnirTest gtest gtest_main)
target_link_libraries(GungnirTest ${LIBS})

add_test(
        NAME GungnirTest
        COMMAND GungnirTest
)
