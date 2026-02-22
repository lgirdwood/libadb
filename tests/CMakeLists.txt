add_executable(test_ngc test_ngc.c)
target_link_libraries(test_ngc PRIVATE astrodb m)
target_include_directories(test_ngc PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_ngc COMMAND test_ngc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_sky2k test_sky2k.c)
target_link_libraries(test_sky2k PRIVATE astrodb m)
target_include_directories(test_sky2k PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_sky2k COMMAND test_sky2k WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
