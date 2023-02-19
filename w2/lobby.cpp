#include <enet/enet.h>
#include <iostream>
#include <cstring>
#include <unordered_set>

int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }
  ENetAddress address;

  address.host = ENET_HOST_ANY;
  address.port = 10887;

  ENetHost *lobby = enet_host_create(&address, 32, 2, 0, 0);

  std::unordered_set<ENetPeer*> players;
  std::string server_port = "p10888";
  bool gameSessionStarted = false;

  if (!lobby)
  {
    printf("Cannot create ENet server\n");
    return 1;
  }

  while (true)
  {
    ENetEvent event;
    while (enet_host_service(lobby, &event, 10) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        if (gameSessionStarted) {
          ENetPacket *packet = enet_packet_create(server_port.data(), server_port.length() + 1, ENET_PACKET_FLAG_RELIABLE);
          enet_peer_send(event.peer, 0, packet);
        }
        players.insert(event.peer);
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        printf("Message recieved: '%s'\n", event.packet->data);
        if (!strcmp((const char*)event.packet->data, "start")) {
          if (!gameSessionStarted) {
            for (const auto& peer: players) {
              ENetPacket *packet = enet_packet_create(server_port.data(), server_port.length() + 1, ENET_PACKET_FLAG_RELIABLE);
              enet_peer_send(peer, 0, packet);
            }
            gameSessionStarted = true;
          }
        }
        enet_packet_destroy(event.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        printf("%x:%u disconnected\n", event.peer->address.host, event.peer->address.port);
        players.erase(event.peer);
        break;
      default:
        break;
      };
    }
  }

  enet_host_destroy(lobby);

  atexit(enet_deinitialize);
  return 0;
}