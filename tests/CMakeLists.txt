set(TLB_TESTS "${PROJECT_NAME}_tests")

file(GLOB TLB_TEST_SOURCES "**.c")

add_executable(${TLB_TESTS} ${TLB_TEST_SOURCES})
target_link_libraries(${TLB_TESTS} ${PROJECT_NAME})
