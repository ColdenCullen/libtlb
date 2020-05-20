enable_language(CXX)

# Get GoogleTest
include(FetchContent)
include(GoogleTest)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.10.0
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()


set(TLB_TESTS "${PROJECT_NAME}_tests")

file(GLOB TLB_TEST_SOURCES "*.cc")

add_executable(${TLB_TESTS} ${TLB_TEST_SOURCES})
target_link_libraries(${TLB_TESTS} ${PROJECT_NAME} gtest gtest_main)
gtest_discover_tests(${TLB_TESTS})
