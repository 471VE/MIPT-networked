#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <iostream>
#include "socket_tools.h"

int main(int argc, const char **argv)
{
  const char *port = "2023";

  int server_sfd = create_dgram_socket(nullptr, port, nullptr);

  if (server_sfd == -1)
    return 1;
  printf("listening!\n");

  const char *client_port = "2049";
  addrinfo clientAddrInfo{};
  int client_sfd = 0;

  while (true)
  {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(server_sfd, &readSet);

    timeval timeout = { 0, 100000 }; // 100 ms
    select(server_sfd + 1, &readSet, NULL, NULL, &timeout);


    if (FD_ISSET(server_sfd, &readSet))
    {
      constexpr size_t buf_size = 1000;
      static char buffer[buf_size];
      memset(buffer, 0, buf_size);

      ssize_t numBytes = recvfrom(server_sfd, buffer, buf_size - 1, 0, nullptr, nullptr);
      
      if (numBytes > 0) {
        const MessageType message_type = static_cast<MessageType>(*buffer);
        char *message_data = buffer + sizeof(MessageType);

        switch (message_type) {
          case CONNECTION_INIT:
            std::cout << message_data << " joined the server!\n";
            client_sfd = create_dgram_socket("localhost", client_port, &clientAddrInfo);
            if (client_sfd == -1)
              return 1;
            break;

          case DATA_TRANSFER:
            std::cout << "Message recieved: " << message_data << '\n';
            if (send_message(client_sfd, DATA_TRANSFER, message_data, clientAddrInfo) != 0)
              return 1;
            break;
          
          case KEEP_ALIVE: break;

          default: std::cout << "Unknown message message_type\n";
        }
      }
    }
  }
  return 0;
}
