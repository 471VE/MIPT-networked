// initial skeleton is a clone from https://github.com/jpcy/bgfx-minimal-example
//
#include <functional>
#include "raylib.h"
#include <enet/enet.h>
#include <math.h>

#include <deque>
#include <vector>
#include "auxilary.h"
#include "entity.h"
#include "protocol.h"


static std::deque<Input> inputHistory;
static std::vector<Entity> entities;
static uint16_t my_entity = invalid_entity;

void on_new_entity_packet(ENetPacket *packet)
{
  Entity newEntity;
  deserialize_new_entity(packet, newEntity);
  // TODO: Direct adressing, of course!
  for (const Entity &e : entities)
    if (e.eid == newEntity.eid)
      return; // don't need to do anything, we already have entity
  entities.push_back(newEntity);
}

void on_set_controlled_entity(ENetPacket *packet)
{
  deserialize_set_controlled_entity(packet, my_entity);
}

void on_snapshot(ENetPacket *packet, ENetPeer* peer)
{
  uint16_t current_snapshot_id;
  deserialize_snapshot(packet, entities, current_snapshot_id);
  send_snapshot_acknowledgement(peer, my_entity, current_snapshot_id);
}

uint16_t on_input_acknowledgement(ENetPacket* packet)
{
  uint16_t new_reference_input_id = 0;
  deserialize_input_acknowledgement(packet, new_reference_input_id);
  return new_reference_input_id;
}

int main(int argc, const char **argv)
{
  uint16_t input_id = 0;
  uint16_t reference_input_id = 0;
  Input input;
  input.id = input_id;
  input.steer = 0;
  input.thr = 0;
  inputHistory.push_back(input);

  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }

  ENetHost *client = enet_host_create(nullptr, 1, 2, 0, 0);
  if (!client)
  {
    printf("Cannot create ENet client\n");
    return 1;
  }

  ENetAddress address;
  enet_address_set_host(&address, "localhost");
  address.port = 10131;

  ENetPeer *serverPeer = enet_host_connect(client, &address, 2, 0);
  if (!serverPeer)
  {
    printf("Cannot connect to server");
    return 1;
  }

  int width = 600;
  int height = 600;

  InitWindow(width, height, "hw6");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  Camera2D camera = { {0, 0}, {0, 0}, 0.f, 1.f };
  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ width * 0.5f, height * 0.5f };
  camera.rotation = 0.f;
  camera.zoom = 10.f;


  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

  bool connected = false;
  while (!WindowShouldClose())
  {
    float dt = GetFrameTime();
    ENetEvent event;
    while (enet_host_service(client, &event, 0) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        send_join(serverPeer);
        connected = true;
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        switch (get_packet_type(event.packet))
        {
        case E_SERVER_TO_CLIENT_NEW_ENTITY:
          on_new_entity_packet(event.packet);
          break;
        case E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY:
          on_set_controlled_entity(event.packet);
          break;
        case E_SERVER_TO_CLIENT_SNAPSHOT:
          on_snapshot(event.packet, event.peer);
          break;
        case E_SERVER_TO_CLIENT_INPUT_ACK:
          reference_input_id = on_input_acknowledgement(event.packet);
          while (inputHistory[0].id != reference_input_id) //clear history
            inputHistory.pop_front();
          break;
        };
        break;
      default:
        break;
      };
    }
    if (my_entity != invalid_entity)
    {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);
      // TODO: Direct adressing, of course!
      for (Entity &e : entities)
        if (e.eid == my_entity)
        {
          // Update
          int thr = 0;
          int steer = 0;
          input.thr = (up ? 1.f : 0.f) + (down ? -1.f : 0.f);
          input.steer = (left ? -1.f : 0.f) + (right ? 1.f : 0.f);
          input.id = ++input_id;
          inputHistory.push_back(input);
          uint8_t header = 0;
          for (Input& input2 : inputHistory)
          {
            if (input2.id == reference_input_id)
            {
              if (input2.steer != input.steer || input2.thr != input.thr)
              {
                steer = (int)input.steer;
                thr = (int)input.thr;
                header = 128;
              }
              break;
            }
          }

          // Send
          send_entity_input(serverPeer, my_entity, input.thr, input.steer, header, input_id, reference_input_id);
          break;
        }
    }

    BeginDrawing();
      ClearBackground(GRAY);
      BeginMode2D(camera);
        DrawRectangleLines(-16, -8, 32, 16, GetColor(0xff00ffff));
        for (const Entity &e : entities)
        {
          const Rectangle rect = {e.x, e.y, 3.f, 1.f};
          DrawRectanglePro(rect, {0.f, 0.5f}, e.ori * 180.f / PI, GetColor(e.color));
        }

      EndMode2D();
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
