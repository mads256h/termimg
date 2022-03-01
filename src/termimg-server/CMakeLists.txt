add_executable(termimg main.cpp term.cpp ipc-server.cpp)
target_link_libraries(termimg PRIVATE project_warnings X11 XRes Imlib2 procps)
