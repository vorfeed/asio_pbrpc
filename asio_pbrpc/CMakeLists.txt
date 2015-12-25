add_subdirectory(net_trans)
add_subdirectory(pbrpc)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/asio_pbrpc)
