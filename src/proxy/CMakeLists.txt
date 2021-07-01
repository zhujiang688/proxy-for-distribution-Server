aux_source_directory(. SRC_LIST)

add_executable(proxy ${SRC_LIST})

target_link_libraries(proxy event crypto jsoncpp pthread log)