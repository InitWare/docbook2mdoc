add_executable(iw-docbook2mdoc EXCLUDE_FROM_ALL docbook2mdoc.c macro.c main.c
	node.c parse.c reorg.c tree.c xmalloc.c)
target_link_libraries(iw-docbook2mdoc compat)