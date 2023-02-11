#include "thread_functions.h"

#include <iostream>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <unistd.h>

void input_function(int client_sfd, addrinfo resAddrInfo) {
  while (true) {
    std::string input;
    std::getline(std::cin, input);
    if (send_message(client_sfd, DATA_TRANSFER, input, resAddrInfo) != 0)
      std::cout << strerror(errno) << std::endl;
  }
}

void keep_alive_function(int client_sfd, addrinfo addrInfo) {
  while (true) {
    sleep(5);
    if (send_message(client_sfd, KEEP_ALIVE, {}, addrInfo) != 0)
      return;
  }
}

void listen_function(int client_sfd) {
  while (true) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(client_sfd, &readSet);

    timeval timeout = { 0, 100000 }; // 100 ms
    select(client_sfd + 1, &readSet, NULL, NULL, &timeout);

    if (FD_ISSET(client_sfd, &readSet)) {
      constexpr size_t buf_size = 1000;
      static char buffer[buf_size];
      memset(buffer, 0, buf_size);

      ssize_t numBytes = recvfrom(client_sfd, buffer, buf_size - 1, 0, nullptr, nullptr);

      if (numBytes > 0) {
        const MessageType message_type = static_cast<MessageType>(*buffer);
        char *message_data = buffer + sizeof(MessageType);

        switch(message_type) {
          case DATA_TRANSFER:
            std::cout << "Server recieved your message: " << message_data << std::endl;
            break;
          default:
            std::cout << "Unknown message type\n";
        }
      }
    }
  }
}
