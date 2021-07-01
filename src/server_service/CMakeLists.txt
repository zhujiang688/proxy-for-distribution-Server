aux_source_directory(. SRC_LIST)

add_executable(server_service ${SRC_LIST})

target_link_libraries(server_service event jsoncpp mysqlclient log)
