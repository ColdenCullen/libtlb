cmake_minimum_required(VERSION 3.10)

project(tlb LANGUAGES C)

option(ENABLE_SANITIZERS "Enable sanitizers in debug builds" ON)
set(SANITIZERS "address;undefined" CACHE STRING "List of sanitizers to build with")

set(TLB_COPTS -Werror -Wall -Wextra)
set(TLB_LDOPTS "")
if(ENABLE_SANITIZERS)
  string(REPLACE ";" "," SANITIZERS "${SANITIZERS}")
  list(APPEND TLB_COPTS "-fsanitize=${SANITIZERS}")
  list(APPEND TLB_LDOPTS "-fsanitize=${SANITIZERS}")
endif()

file(GLOB TLB_SOURCES "source/*.c")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(GLOB TLB_SOURCES_PLATFORM "source/linux/*.c")
elseif(CMAKE_SYSTEM_NAME MATCHES ".*BSD" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  file(GLOB TLB_SOURCES_PLATFORM "source/bsd/*.c")
endif()

add_library(${PROJECT_NAME} ${TLB_SOURCES} ${TLB_SOURCES_PLATFORM})
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_options(${PROJECT_NAME} PRIVATE ${TLB_COPTS})
target_link_options(${PROJECT_NAME} PUBLIC ${TLB_LDOPTS})

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
