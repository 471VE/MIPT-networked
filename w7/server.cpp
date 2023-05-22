#include <enet/enet.h>
#include <iostream>
#include "auxilary.h"
#include "entity.h"
#include "protocol.h"
#include "mathUtils.h"
#include <stdlib.h>
#include <vector>
#include <map>

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

static std::vector<Entity> entities;
static std::map<uint16_t, ENetPeer*> controlledMap;
static std::vector<Snapshot> snapshotHistory;
static std::map<uint16_t, uint16_t> approvedReferenceSnapshotMap;

void on_join(ENetPacket *packet, ENetPeer *peer, ENetHost *host)
{
  // send all entities
  for (const Entity &ent : entities)
    send_new_entity(peer, ent);

  // find max eid
  uint16_t maxEid = entities.empty() ? invalid_entity : entities[0].eid;
  for (const Entity &e : entities)
    maxEid = std::max(maxEid, e.eid);
  uint16_t newEid = maxEid + 1;
  uint32_t color = 0xff000000 +
                   0x00ff0000 * (rand() % 5) +
                   0x0000ff00 * (rand() % 5) +
                   0x000000ff * (rand() % 5);
  float x = (rand() % 4) * 5.f;
  float y = (rand() % 4) * 5.f;
  Entity ent = {color, x, y, 0.f, (rand() / RAND_MAX) * 3.141592654f, 0.f, 0.f, newEid};
  entities.push_back(ent);

  controlledMap[newEid] = peer;
  approvedReferenceSnapshotMap[newEid] = 0;


  // send info about new entity to everyone
  for (size_t i = 0; i < host->peerCount; ++i)
    send_new_entity(&host->peers[i], ent);
  // send info about controlled entity
  send_set_controlled_entity(peer, newEid);
}

void on_input(ENetPacket* packet, ENetPeer* peer)
{
  uint16_t eid = invalid_entity;
  float thr = 0.f; float steer = 0.f;
  uint16_t new_reference_input_id = 0;
  deserialize_entity_input(packet, eid, thr, steer, new_reference_input_id);
  for (Entity &e : entities)
    if (e.eid == eid)
    {
      e.thr = thr;
      e.steer = steer;
    }
  send_input_acknowledgement(peer, new_reference_input_id);
}

void on_snapshot_acknowledgement(ENetPacket* packet)
{
  uint16_t eid, current_snapshot_id;
  deserialize_snapshot_acknowledgement(packet, eid, current_snapshot_id);
  approvedReferenceSnapshotMap[eid] = current_snapshot_id;
}

int main(int argc, const char **argv)
{
  hw_test();

  Snapshot snapshot;
  snapshot.entities = entities;
  snapshot.id = 0;

  std::map<uint16_t, uint8_t> headers;

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

  uint32_t lastTime = enet_time_get();
  while (true)
  {
    uint32_t curTime = enet_time_get();
    float dt = (curTime - lastTime) * 0.001f;
    lastTime = curTime;
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
            on_join(event.packet, event.peer, server);
            break;
          case E_CLIENT_TO_SERVER_INPUT:
            on_input(event.packet, event.peer);
            break;
          case E_CLIENT_TO_SERVER_SNAPSHOT_ACK:
            on_snapshot_acknowledgement(event.packet);
            break;
        };
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }
    static int t = 0;
    for (Entity &e : entities)
    {
      // simulate
      simulate_entity(e, dt);
    }

    snapshot.entities = entities;
    snapshot.id++;
    snapshotHistory.push_back(snapshot);

    if(!entities.empty())
    {
      for (Entity& ent : entities)
      {
        headers.clear();
        ENetPeer* peer = controlledMap[ent.eid];
        for (Entity& e1 : entities)
        {
          uint8_t header = 0;
          for (Entity& e2 : snapshotHistory[approvedReferenceSnapshotMap[ent.eid]].entities)
          {
            if (e2.eid == e1.eid)
            {
              if (e1.x != e2.x || e1.y != e2.y)
                header = header | 0b11000000;
              if (e1.ori != e2.ori)
                header = header | 0b10100000;
              break;
            }
          }
          headers[e1.eid] = header;
        }

        send_snapshot(peer, entities, headers, snapshot.id, approvedReferenceSnapshotMap[ent.eid]);
      }
    }
    usleep(10000);
  }

  enet_host_destroy(server);

  atexit(enet_deinitialize);
  return 0;
}


