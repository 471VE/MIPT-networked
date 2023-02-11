#pragma once

#include "socket_tools.h"

void input_function(int client_sfd, addrinfo resAddrInfo);
void keep_alive_function(int client_sfd, addrinfo addrInfo);
void listen_function(int client_sfd);
