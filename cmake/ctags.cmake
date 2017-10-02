# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4: #

find_program(CTAGS ctags)

if(CTAGS)
	add_custom_target(gen_ctags ALL ${CTAGS} -R ${CMAKE_SOURCE_DIR})
endif()
