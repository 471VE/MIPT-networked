#include <enet/enet.h>
#include <iostream>
#include "entity.h"
#include "protocol.h"
#include "mathUtils.h"
#include <stdlib.h>
#include <vector>
#include <map>
#include "auxilary.h"

static std::map<uint16_t, Entity> entities;
static std::map<uint16_t, ENetPeer*> controlledMap;

#ifdef WIN32
  void usleep(__int64 usec) 
  { 
    HANDLE timer; 
    LARGE_INTEGER ft;
    ft.QuadPart = -(10*usec);
    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
  }
#endif

void on_join(ENetPeer *peer, ENetHost *host, uint32_t timestamp)
{
  // find max eid
  uint16_t maxEid = entities.empty() ? invalid_entity : entities[0].eid;
  for (const auto &[eid, e] : entities)
    maxEid = std::max(maxEid, eid);
  uint16_t newEid = maxEid + 1;
  uint32_t color = 0xff000000 +
                   0x00ff0000 * (rand() % 5) +
                   0x0000ff00 * (rand() % 5) +
                   0x000000ff * (rand() % 5);
  float x = (rand() % 4) * 5.f;
  float y = (rand() % 4) * 5.f;
  Entity ent = {color, x, y, 0.f, (rand() / RAND_MAX) * 3.141592654f, 0.f, 0.f, newEid, timestamp};
  for (const auto& [eid, e] : entities)
    send_new_entity(peer, e);

  entities[newEid] = ent;
  controlledMap[newEid] = peer;


  // send info about new entity to everyone
  for (size_t i = 0; i < host->peerCount; ++i)
    if (host->peers[i].channels)
      send_new_entity(&host->peers[i], ent);
  // send info about controlled entity
  send_set_controlled_entity(peer, newEid);
}

void on_input(ENetPacket *packet)
{
  uint16_t eid = invalid_entity;
  float thr = 0.f; float steer = 0.f;
  deserialize_entity_input(packet, eid, thr, steer);
  Entity &e = entities[eid];
  e.thr = thr;
  e.steer = steer;
}

void on_snapshot(ENetPacket* packet)
{
  uint16_t eid;
  float x, y, ori;
  uint32_t timestamp;
  deserialize_snapshot(packet, eid, x, y, ori, timestamp);
  entities[eid].x = x;
  entities[eid].y = y;
  entities[eid].ori = ori;
}

int main(int argc, const char **argv)
{
  srand(10);
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }
  ENetAddress address;

  address.host = ENET_HOST_ANY;
  address.port = 10131;

  ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);

  if (!server)
  {
    printf("Cannot create ENet server\n");
    return 1;
  }

  while (true)
  {
    uint32_t curTime = enet_time_get();
    ENetEvent event;
    while (enet_host_service(server, &event, 0) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        switch (get_packet_type(event.packet))
        {
          case E_CLIENT_TO_SERVER_JOIN:
            on_join(event.peer, server, convert_to_timestamp(curTime));
            break;
          case E_CLIENT_TO_SERVER_INPUT:
            on_input(event.packet);
            break;
          case E_CLIENT_TO_SERVER_SNAPSHOT:
            on_snapshot(event.packet);
            break;
        };
        enet_packet_destroy(event.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        printf("Disconnect with %x:%u\n", event.peer->address.host, event.peer->address.port);
        break;
      default:
        break;
      };
    }
  for (auto& [eid, e] : entities)
  {
    while (e.timestamp < convert_to_timestamp(curTime))
    {
      simulate_entity(e, dt);
      e.timestamp++;
    }
      // send
    for (size_t i = 0; i < server->peerCount; ++i)
    {
      ENetPeer* peer = &server->peers[i];
      if (peer->channels)
        send_snapshot(peer, E_SERVER_TO_CLIENT_SNAPSHOT, e.eid, e.x, e.y, e.ori, e.timestamp);
    }
  }
    usleep(10000);
  }

  enet_host_destroy(server);

  atexit(enet_deinitialize);
  return 0;
}


