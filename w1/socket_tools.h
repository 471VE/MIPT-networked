#pragma once

#include <string>

struct addrinfo;

int create_dgram_socket(const char *address, const char *port, addrinfo *res_addr);

enum MessageType {
    CONNECTION_INIT,
    DATA_TRANSFER,
    KEEP_ALIVE
};

int send_message(int fd, MessageType type, std::string message, addrinfo addrInfo);
