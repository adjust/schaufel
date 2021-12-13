set (files
    schaufel.1
    schaufel.conf.5
)


add_custom_target(docs)

foreach(file ${files})
add_custom_command(
    TARGET docs
    COMMAND
    groff -mandoc -f H -T ps ${CMAKE_SOURCE_DIR}/man/${file} | ps2pdf - ${file}.pdf
    DEPENDS ${file}
)
endforeach()
