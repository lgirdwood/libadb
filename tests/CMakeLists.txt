# Copy test catalog data to the build directory so tests can run independently
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/VII DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/V DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)

add_executable(test_ngc test_ngc.c)
target_link_libraries(test_ngc PRIVATE astrodb m)
target_include_directories(test_ngc PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_ngc COMMAND test_ngc WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_sky2k test_sky2k.c)
target_link_libraries(test_sky2k PRIVATE astrodb m)
target_include_directories(test_sky2k PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_sky2k COMMAND test_sky2k WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_htm test_htm.c)
target_link_libraries(test_htm PRIVATE astrodb m)
target_include_directories(test_htm PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_htm COMMAND test_htm WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_all test_all.c)
target_compile_definitions(test_all PRIVATE 
    TEST_NGC_PATH=\"$<TARGET_FILE:test_ngc>\"
    TEST_SKY2K_PATH=\"$<TARGET_FILE:test_sky2k>\"
    TEST_HTM_PATH=\"$<TARGET_FILE:test_htm>\"
)
add_test(NAME test_suite_all COMMAND test_all WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
