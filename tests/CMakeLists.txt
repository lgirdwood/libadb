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

add_executable(test_kdtree test_kdtree.c)
target_link_libraries(test_kdtree PRIVATE astrodb m)
target_include_directories(test_kdtree PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_kdtree COMMAND test_kdtree WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_hash test_hash.c)
target_link_libraries(test_hash PRIVATE astrodb m)
target_include_directories(test_hash PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_hash COMMAND test_hash WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_import test_import.c)
target_link_libraries(test_import PRIVATE astrodb m)
target_include_directories(test_import PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_import COMMAND test_import WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_schema test_schema.c)
target_link_libraries(test_schema PRIVATE astrodb m)
target_include_directories(test_schema PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_schema COMMAND test_schema WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_table test_table.c)
target_link_libraries(test_table PRIVATE astrodb m)
target_include_directories(test_table PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_table COMMAND test_table WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_solve test_solve.c)
target_link_libraries(test_solve PRIVATE astrodb m)
target_include_directories(test_solve PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_solve COMMAND test_solve WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_all test_all.c)
target_compile_definitions(test_all PRIVATE 
    TEST_NGC_PATH=\"$<TARGET_FILE:test_ngc>\"
    TEST_SKY2K_PATH=\"$<TARGET_FILE:test_sky2k>\"
    TEST_HTM_PATH=\"$<TARGET_FILE:test_htm>\"
    TEST_KDTREE_PATH=\"$<TARGET_FILE:test_kdtree>\"
    TEST_HASH_PATH=\"$<TARGET_FILE:test_hash>\"
    TEST_IMPORT_PATH=\"$<TARGET_FILE:test_import>\"
    TEST_SCHEMA_PATH=\"$<TARGET_FILE:test_schema>\"
    TEST_TABLE_PATH=\"$<TARGET_FILE:test_table>\"
    TEST_SOLVE_PATH=\"$<TARGET_FILE:test_solve>\"
)
add_test(NAME test_suite_all COMMAND test_all WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
