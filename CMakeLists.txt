cmake_minimum_required(VERSION 3.10)

project(tlb LANGUAGES C)

file(GLOB TLB_SOURCES "source/*.c")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(GLOB TLB_SOURCES_PLATFORM "source/linux/*.c")
elseif(CMAKE_SYSTEM_NAME MATCHES ".*BSD" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  file(GLOB TLB_SOURCES_PLATFORM "source/bsd/*.c")
endif()

add_library(${PROJECT_NAME} ${TLB_SOURCES} ${TLB_SOURCES_PLATFORM})
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
target_include_directories(${PROJECT_NAME} PUBLIC include)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
