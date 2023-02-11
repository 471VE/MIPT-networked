#include <netdb.h>
#include <cstdio>
#include <iostream>
#include <thread>

#include <unistd.h>

#include "socket_tools.h"
#include "thread_functions.h"

int main(int argc, const char **argv)
{
  std::string client_name;
  
  if (argc == 2)
    client_name = argv[1];
  else if (argc == 1)
    client_name = "Anonymous";
  else {
    std::cout << "Only one additional argument is supported. Exiting...\n";
    return 1;
  }

  const char *client_port = "2023";

  addrinfo resAddrInfo;
  int client_sfd = create_dgram_socket("localhost", client_port, &resAddrInfo);

  if (client_sfd == -1)
  {
    printf("Cannot create a socket\n");
    return 1;
  }

  if (send_message(client_sfd, CONNECTION_INIT, client_name, resAddrInfo) != 0)
    return 1;

  const char *server_port = "2049";
  int server_sfd = create_dgram_socket(nullptr, server_port, nullptr);

  if (server_sfd == -1) {
    std::cout << "Couldn't create socket for listening\n";
    return 1;
  }

  std::cout << "Hi, " << client_name << "! Type your message and press ENTER to send it to the server:" << std::endl;
  std::thread input_thread(input_function, client_sfd, resAddrInfo);
  std::thread listen_thread(listen_function, server_sfd);
  std::thread keep_alive_thread(keep_alive_function, client_sfd, resAddrInfo);

  input_thread.join();
  listen_thread.join();
  keep_alive_thread.join();
  return 0;
}
