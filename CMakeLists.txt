cmake_minimum_required(VERSION 3.10)

project(tlb LANGUAGES C)

option(ENABLE_SANITIZERS "Enable sanitizers in debug builds" ON)
set(SANITIZERS "address;undefined" CACHE STRING "List of sanitizers to build with")

# Disable clang tidy in build directory
file(WRITE "${CMAKE_BINARY_DIR}/.clang-tidy" "Checks: 'clang-*,-clang-analyzer-security.insecureAPI'")

set(TLB_DEFINES "")
set(TLB_COPTS -Werror -Wall -Wextra)
set(TLB_LIBS "")
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

# Check for threads.h
include(CheckIncludeFiles)
CHECK_INCLUDE_FILES("threads.h" TLB_HAS_THREADS_H)
if(TLB_HAS_THREADS_H)
  message(STATUS "Using threads.h")
  list(APPEND TLB_DEFINES "TLB_HAS_THREADS_H")
else()
  message(STATUS "Using TinyCThreads")
  set(TINYCTHREAD_DISABLE_TESTS OFF)
  # Get TinyCThreads
  include(FetchContent)
  FetchContent_Declare(
    tinycthread
    GIT_REPOSITORY https://github.com/tinycthread/tinycthread.git
  )
  FetchContent_GetProperties(tinycthread)
  if(NOT tinycthread_POPULATED)
    FetchContent_Populate(tinycthread)
    add_subdirectory(${tinycthread_SOURCE_DIR} ${tinycthread_BINARY_DIR})
  endif()
  list(APPEND TLB_LIBS "tinycthread")
endif()

add_library(${PROJECT_NAME} ${TLB_SOURCES} ${TLB_SOURCES_PLATFORM})
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
target_include_directories(${PROJECT_NAME} PUBLIC include)

target_compile_definitions(${PROJECT_NAME} PUBLIC ${TLB_DEFINES})
target_compile_options(${PROJECT_NAME} PRIVATE ${TLB_COPTS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${TLB_LIBS})
target_link_options(${PROJECT_NAME} PUBLIC ${TLB_LDOPTS})

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
