add_executable(test_ngc test_ngc.c)

# Link the core library and mathematical library
target_link_libraries(test_ngc PRIVATE astrodb m)

# Make sure it can find libastrodb headers
target_include_directories(test_ngc PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Add it to CTest, executing from the root directory so the "tests" relative path holds
add_test(NAME test_ngc COMMAND test_ngc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
