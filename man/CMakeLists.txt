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
install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/${file}.pdf
	DESTINATION share/doc/schaufel
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

if(${file} MATCHES ".+\.1")
	install(
		FILES ${file}
		DESTINATION man/man1
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	)
endif()
if(${file} MATCHES ".+\.5")
	install(
		FILES ${file}
		DESTINATION man/man5
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	)
endif()
endforeach()
