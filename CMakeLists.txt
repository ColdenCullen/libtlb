cmake_minimum_required(VERSION 3.2)

project(tlb LANGUAGES C CXX)

file(GLOB TLB_SOURCES "source/*.c")

message(STATUS "PLATFORM: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(GLOB TLB_SOURCES_PLATFORM "source/linux/*.c")
elseif(CMAKE_SYSTEM_NAME MATCHES ".*BSD" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  file(GLOB TLB_SOURCES_PLATFORM "source/bsd/*.c")
endif()

add_library(${PROJECT_NAME} ${TLB_SOURCES} ${TLB_SOURCES_PLATFORM})
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE source)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
