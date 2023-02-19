#include <enet/enet.h>
#include <unordered_map>
#include <vector>
#include <string>

static size_t namesNum = 6;
std::vector<std::string> first_names = {"James", "Mary", "Robert", "Patricia", "John", "Jennifer"};
std::vector<std::string> last_names = {"Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia"};

static size_t currentID = 0;
static size_t namesExist = 0;

std::string getName() { 
    size_t first_name_idx = namesExist / namesNum;
    size_t last_name_idx = namesExist % namesNum;
    namesExist++;  
    return first_names[first_name_idx] + " " + last_names[last_name_idx];
} 

struct Client {
  std::string name;
  size_t id;
};

using ClientDatabase = std::unordered_map<ENetPeer*, Client>;

void send_pings(ENetPeer *peerToSend, const ClientDatabase &clients) {
    std::string ping_info = "Client pings:";
    for (const auto &[peer, info] : clients)
        if (peer != peerToSend)
          ping_info += '\n' + info.name + ": " + std::to_string(peer->roundTripTime);

    if (clients.size() > 1) {
        ENetPacket *packet = enet_packet_create(ping_info.data(), ping_info.length() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(peerToSend, 1, packet);
    }
}

void player_joined(ENetPeer* new_peer, ClientDatabase& clients)
{
    Client new_player{getName(), currentID++};
    std::string msg = "New client joined:\n" + new_player.name + ", id: " + std::to_string(new_player.id) + '\n';

    std::string playersTable = "Current clients:";
    for (const auto& client : clients) {
        playersTable += '\n' + client.second.name + ", id: " + std::to_string(client.second.id) ;
        ENetPacket *packet = enet_packet_create(msg.data(), msg.length() + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(client.first, 0, packet);
    }

    ENetPacket *packet = nullptr;
    if (!clients.empty()) {
        packet = enet_packet_create(playersTable.data(), playersTable.length() + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(new_peer, 0, packet);
    }
    clients[new_peer] = new_player;
    printf("Assigned name: %s\nAssigned id: %zu\n\n", new_player.name.data(), new_player.id);
}

int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }

  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = 10888;

  ENetHost *server = enet_host_create(&address, 36, 2, 0, 0);
  if (!server)
  {
    printf("Cannot create ENet server\n");
    return 1;
  }
  printf("Server started...\n");

  ClientDatabase clients;

  uint32_t timeStart = enet_time_get();
  uint32_t lastTimeSend = timeStart;

  while (true) {
    ENetEvent event;
    while (enet_host_service(server, &event, 10) > 0) {
      switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
          printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
          player_joined(event.peer, clients);
          break;
        case ENET_EVENT_TYPE_RECEIVE:
          printf("%s#%zu sent a message:\n'%s'\n\n", clients[event.peer].name.data(), clients[event.peer].id, event.packet->data);
          enet_packet_destroy(event.packet);
          break;
        case ENET_EVENT_TYPE_DISCONNECT:
          printf("%x:%u disconnected\n", event.peer->address.host, event.peer->address.port);
          clients.erase(event.peer);
          break;
        default:
          break;
      };
    }
    if (!clients.empty()) {
      uint32_t curTime = enet_time_get();
      if (curTime - lastTimeSend > 3000) {
        for (const auto& client : clients) {
          std::string time =  "Server time: " + std::to_string(curTime);
          ENetPacket *packet = enet_packet_create(time.data(), time.length() + 1, ENET_PACKET_FLAG_RELIABLE);
          enet_peer_send(client.first, 0, packet);
          lastTimeSend = curTime;
          send_pings(client.first, clients);
        }
      }
    }
  }

  enet_host_destroy(server);

  atexit(enet_deinitialize);
  return 0;
}