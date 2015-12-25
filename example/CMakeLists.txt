aux_source_directory(. example_SRCS)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(rpc.pb.cc rpc.pb.h rpc.proto)
add_library(example ${example_SRCS} rpc.pb.cc rpc.pb.h)

target_link_libraries(
	example
	pthread rt
	protobuf
	boost_thread
	boost_system
	boost_filesystem
	boost_date_time
	boost_program_options
	boost_regex
)

add_executable(server server.cpp)
target_link_libraries(server example)

add_executable(sync_client sync_client.cpp)
target_link_libraries(sync_client example)

add_executable(async_client async_client.cpp)
target_link_libraries(async_client example)

add_executable(future_client future_client.cpp)
target_link_libraries(future_client example)
