#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

const char socket_name[] = "/tmp/termimg";
char message[] = "Hello world!";


int main() {

  int unix_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (unix_socket < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }

  struct sockaddr_un socket_address;
  socket_address.sun_family = AF_UNIX;
  strncpy(socket_address.sun_path, socket_name, sizeof(socket_address.sun_path));

  if (sendto(unix_socket, message, sizeof(message), 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)) < 0) {
    perror("sendto");
    close(unix_socket);
    return EXIT_FAILURE;
  }

  close(unix_socket);

  return EXIT_SUCCESS;
}
