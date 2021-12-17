set(tests
    array
    config_test
    dummy_producer_test
    fnv_test
    jsonexports_test
    logparse_test
    parse_connstring
    queue_test
    bintree_test
    config_merge_test
    dummy_consumer_test
    file_consumer_test
    hooks_test
    logger_test
    metadata_test
    parse_hostinfo
    strlwr_test
)

foreach(testname ${tests})
    add_executable(${testname}_testrunner ${testname}.c)
    target_include_directories(${testname}_testrunner
        PRIVATE
        "$<TARGET_PROPERTY:schaufel,INTERFACE_INCLUDE_DIRECTORIES>"
    )
    target_link_libraries(${testname}_testrunner PRIVATE schaufel_lib $<FILTER:$<TARGET_OBJECTS:schaufel>,EXCLUDE,.*main.c.o>)
    target_compile_options(${testname}_testrunner PRIVATE
        -Wall -Wextra -pedantic
        -DTESTING
    )
    add_test(${testname} ${testname}_testrunner)
endforeach()
