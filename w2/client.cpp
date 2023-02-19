#include <enet/enet.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <string>

void input_async(ENetPeer* lobby, ENetPeer* server) {
  while (!server) {
    std::string input;
    std::getline(std::cin, input);

    if (!strcmp(input.data(), "start")) {
      printf("Starting the game...\n");
      const char *msg = "start";
      ENetPacket *packet = enet_packet_create(msg, strlen(msg) + 1, ENET_PACKET_FLAG_RELIABLE);
      enet_peer_send(lobby, 0, packet);
      break;
    }
  }
}

int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }

  ENetHost *client = enet_host_create(nullptr, 2, 2, 0, 0);
  if (!client)
  {
    printf("Cannot create ENet client\n");
    return 1;
  }

  ENetAddress address_lobby;
  enet_address_set_host(&address_lobby, "localhost");
  address_lobby.port = 10887;

  ENetPeer *lobbyPeer = enet_host_connect(client, &address_lobby, 2, 0);
  if (!lobbyPeer)
  {
    printf("Cannot connect to lobby");
    return 1;
  }

  ENetPeer *serverPeer = nullptr;
  std::thread input_thread(input_async, lobbyPeer, serverPeer);

  uint32_t timeStart = enet_time_get();
  uint32_t lastTimeSend = timeStart;
  while (true)
  {
    ENetEvent event;
    while (enet_host_service(client, &event, 10) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        printf("Message received:\n'%s'\n\n", event.packet->data);
        if (event.packet->data[0] == 'p') {
          uint16_t port = std::atoi((const char *)(event.packet->data + 1));

          ENetAddress address_server;
          enet_address_set_host(&address_server, "localhost");
          address_server.port = port;

          serverPeer = enet_host_connect(client, &address_server, 2, 0);
          if (!serverPeer) {
            printf("Cannot connect to a server");
            return 1;
          }
          printf("Connecting to game server on port %u\n", port);
        }
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }
    if (serverPeer)
    {
      uint32_t curTime = enet_time_get();
      if (curTime - lastTimeSend > 3000)
      {
        std::string time = "Client time: " + std::to_string(curTime);
        ENetPacket *packet = enet_packet_create(time.data(), time.length() + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(serverPeer, 0, packet);
        lastTimeSend = curTime;
      }
    }
  }
  enet_host_destroy(client);

  atexit(enet_deinitialize);
  input_thread.join();
  return 0;
}