set(EXAMPLE_EXECUTABLES
    sky2k
    ngc
    hyperleda
    tycho
    gsc
)

foreach(example_name IN LISTS EXAMPLE_EXECUTABLES)
    add_executable(${example_name} ${example_name}.c)
    target_link_libraries(${example_name} astrodb)
    # The original Makefile.am linked against ../src/libastrodb.la
    # Our astrodb target automatically propagates include directories.
endforeach()
