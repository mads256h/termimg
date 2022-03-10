add_executable(termimg-server main.cpp term.cpp ipc-server.cpp epoll.cpp terminal-info.cpp)
target_link_libraries(termimg-server PRIVATE project_warnings X11 XRes Imlib2 procps)
