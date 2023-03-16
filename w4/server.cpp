#include <enet/enet.h>
#include <iostream>
#include "entity.h"
#include "protocol.h"
#include <stdlib.h>
#include <vector>
#include <map>
#include <random>
#include <chrono>
#include <thread>
#include "raymath.h"

void usleep(__int64 usec) 
{ 
    HANDLE timer; 
    LARGE_INTEGER ft; 

    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
}

static std::vector<Entity> entities;
static std::map<uint16_t, ENetPeer*> controlledMap;
static std::map<uint16_t, Vector2> targets;

std::random_device rd;
std::default_random_engine gen( rd() );

const float POSITION_RANGE = 300.f;
const float PLAYER_POSITION_RANGE = 50.f;
const float MIN_SIZE = 5.f;
const float MAX_SIZE = 20.f;

Vector2 generatePosition() {
  std::uniform_real_distribution<float> uniform(-POSITION_RANGE, POSITION_RANGE);
  return Vector2(uniform(gen), uniform(gen));
}

Vector2 generatePlayerPosition() {
  std::uniform_real_distribution<float> uniform(-PLAYER_POSITION_RANGE, PLAYER_POSITION_RANGE);
  return Vector2(uniform(gen), uniform(gen));
}

Color generateColor() {
  std::uniform_int_distribution<int> uniform(0, 255);
  return Color(uniform(gen), uniform(gen), uniform(gen), 255);
}

float generateSize() {
  std::uniform_real_distribution<float> uniform(MIN_SIZE, MAX_SIZE);
  return uniform(gen);
}

const uint16_t FPS = 60;

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
  Color color = generateColor();
  Vector2 position = generatePlayerPosition();
  float size = generateSize();
  Entity ent = {color, position, size, newEid};
  entities.push_back(ent);

  controlledMap[newEid] = peer;

  // send info about new entity to everyone
  for (size_t i = 0; i < host->peerCount; ++i)
    send_new_entity(&host->peers[i], ent);
  // send info about controlled entity
  send_set_controlled_entity(peer, newEid);
}

void on_state(ENetPacket *packet)
{
  uint16_t eid = invalid_entity;
  Vector2 position;
  deserialize_entity_state(packet, eid, position);
  for (Entity &e : entities)
    if (e.eid == eid)
    {
      e.position = position;
    }
}

void generateAiEntities()
{
  for (uint16_t i = 0; i < 10; ++i)
  {
    Color color = generateColor();
    Vector2 position = generatePosition();
    float size = generateSize();    
    Entity ent = {color, position, size, i};
    entities.push_back(ent);
    Vector2 target = generatePosition();
    targets[i] = target;
  }
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
  address.port = 10131;

  ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);

  if (!server)
  {
    printf("Cannot create ENet server\n");
    return 1;
  }

  generateAiEntities();

  while (true)
  {
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
          case E_CLIENT_TO_SERVER_STATE:
            on_state(event.packet);
            break;
        };
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }
    static int t = 0;
    for (Entity &e : entities) {
      for (Entity &e2 : entities) {
        if (e2.eid != e.eid) {
          if (Vector2Distance(e.position, e2.position) < e.size + e2.size) {
            if (e.size > e2.size) {
              e.size = std::min(e.size + e2.size / 2.f, 50.f);
              e2.size = std::max(e2.size / 2.f, 5.f);
              e2.position = generatePosition();

              if (controlledMap.contains(e2.eid))
                send_snapshot(controlledMap[e2.eid], e2.eid, e2.position, e2.size);
              if (controlledMap.contains(e.eid))
                send_snapshot(controlledMap[e.eid], e.eid, e.position, e.size);
            }
          }
        }
      }

      if (targets.contains(e.eid)) {
        if (Vector2Distance(targets[e.eid], e.position) < 1.f) {
          Vector2 target = generatePosition();
          targets[e.eid] = target;
        }
        e.position = Vector2Add(e.position, Vector2Scale(Vector2Normalize(Vector2Subtract(targets[e.eid], e.position)), 100.f / FPS));
      }

      for (size_t i = 0; i < server->peerCount; ++i)
      {
        ENetPeer *peer = &server->peers[i];
        if (!controlledMap.contains(e.eid) || controlledMap[e.eid] != peer)
          send_snapshot(peer, e.eid, e.position, e.size);
      }
    }
    usleep(static_cast<uint32_t>(1000000.f / FPS));
  }

  enet_host_destroy(server);

  atexit(enet_deinitialize);
  return 0;
}
