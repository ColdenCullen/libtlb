cmake_minimum_required(VERSION 3.2)

project(tlb LANGUAGES C CXX)

file(GLOB TLB_SOURCES "source/**.c")

add_library(${PROJECT_NAME} ${TLB_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC include)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
