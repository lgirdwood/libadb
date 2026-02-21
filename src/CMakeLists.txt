add_library(astrodb SHARED
    cds_file.c
    cds_parse.c
    db.c
    htm_core.c
    htm_file.c
    htm_get.c
    import.c
    htm_insert.c
    htm_import.c
    kd_tree.c
    schema.c
    search.c
    table.c
    hash.c
    solve.c
    solve_pa.c
    solve_dist.c
    solve_mag.c
    solve_target.c
    astrometry.c
    photometry.c
    solution.c
)

# Set library versioning
set_target_properties(astrodb PROPERTIES
    VERSION ${LIBASTRODB_CURRENT_VERSION}.${LIBASTRODB_RELEASE_VERSION}.${LIBASTRODB_AGE_VERSION}
    SOVERSION ${LIBASTRODB_CURRENT_VERSION}
)

# Additional compile options
if(ENABLE_AVX)
    target_compile_options(astrodb PRIVATE -mavx)
endif()

if(ENABLE_OPENMP)
    target_compile_options(astrodb PRIVATE ${OPENMP_CFLAGS})
endif()

# Autotools provided these globally
target_compile_definitions(astrodb PRIVATE
    VERSION="${LIBASTRODB_VERSION}"
    PACKAGE_NAME="${PROJECT_NAME}"
)

# Dependencies
target_link_libraries(astrodb m z ftp)

# Include directories
target_include_directories(astrodb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Install rules
install(TARGETS astrodb
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    libastrodb/astrodb.h
    libastrodb/solve.h
    libastrodb/search.h
    libastrodb/db.h
    libastrodb/db-import.h
    libastrodb/object.h
    DESTINATION include/libastrodb
)
