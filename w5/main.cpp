// initial skeleton is a clone from https://github.com/jpcy/bgfx-minimal-example
//
#include <functional>
#include "raylib.h"
#include <enet/enet.h>
#include <math.h>

#include <vector>
#include "entity.h"
#include "protocol.h"
#include "auxilary.h"
#include <unordered_map>
#include <deque>

static std::unordered_map<uint16_t, Entity> entities;
static uint16_t my_entity = invalid_entity;
static std::unordered_map<uint16_t, Snapshot> entities_last_snapshots;
static std::unordered_map<uint16_t, std::deque<Snapshot>> entities_snapshots;
static std::vector<Input> inputs;
static std::vector<Snapshot> this_entity_snapshots;

const uint32_t offset = 100u;

void on_new_entity_packet(ENetPacket *packet)
{
  Entity newEntity;
  deserialize_new_entity(packet, newEntity);
  if (entities.find(newEntity.eid) == std::end(entities))
  {
    entities[newEntity.eid] = newEntity;
    entities_last_snapshots[newEntity.eid] = {newEntity.x, newEntity.y, newEntity.ori, enet_time_get()};
    entities_snapshots[newEntity.eid] = std::deque<Snapshot>();
  }
}

void on_set_controlled_entity(ENetPacket *packet)
{
  deserialize_set_controlled_entity(packet, my_entity);
}

void simulate_this_entity(const Input& input, uint32_t timestamp)
{
  Entity &this_entity = entities[my_entity];
  this_entity.thr = input.thr;
  this_entity.steer = input.steer;
  for (uint32_t t = 0; t < timestamp; ++t)
  {
    simulate_entity(this_entity, dt);
    this_entity.timestamp++;
    this_entity_snapshots.emplace_back(this_entity.x, this_entity.y, this_entity.ori, this_entity.timestamp);
  }
}

void recover(uint32_t timestampBegin, uint32_t timestampEnd)
{
  auto inputs_to_erase = inputs.begin();
  for (auto input = inputs.begin(); (input < std::end(inputs) && input->thr < timestampEnd); ++input)
  {
    if (input->timestamp >= timestampBegin)
    {
      if (input + 1 < std::end(inputs))
        simulate_this_entity(*input, (input+1)->timestamp - input->timestamp);
      else
        simulate_this_entity(*input, timestampEnd - input->timestamp);
    }
    else
      inputs_to_erase = input;
  }
  inputs.erase(inputs.begin(), inputs_to_erase);
}

void on_snapshot(ENetPacket *packet)
{
  uint16_t eid = invalid_entity;
  float x = 0.f; float y = 0.f; float ori = 0.f;
  uint32_t timestamp;
  deserialize_snapshot(packet, eid, x, y, ori, timestamp);
  if (eid != my_entity)
  {
    if (entities_snapshots.find(eid) != std::end(entities_snapshots))
      entities_snapshots[eid].emplace_back(x, y, ori, enet_time_get() + offset);
  }
  else
  {
    size_t i = 0;
    while (i < this_entity_snapshots.size())
    {
      Snapshot &snapshot = this_entity_snapshots[i];
      if (snapshot.timestamp > timestamp)
        break;
      if (snapshot.timestamp == timestamp && !(snapshot.x == x && snapshot.y == y && snapshot.ori == ori))
      {
        Entity &this_entity = entities[my_entity];
        this_entity.x = x;
        this_entity.y = y;
        this_entity.ori = ori;
        uint32_t oldTimestamp = this_entity.timestamp;
        this_entity.timestamp = timestamp;
        recover(timestamp, oldTimestamp);
        i = this_entity_snapshots.size();
        break;
      }
      i++;
    }
    this_entity_snapshots.erase(this_entity_snapshots.begin(), this_entity_snapshots.begin() + i);
  }
}

void interpolate_entities()
{
  for (auto &[eid, snapshot1] : entities_last_snapshots)
  {
    if (eid == my_entity)
      continue;
    auto curTime = enet_time_get();
    while (!entities_snapshots[eid].empty())
    {
      Snapshot &snapshot2 = entities_snapshots[eid].front();
      if (curTime >= snapshot2.timestamp)
      {
        entities_last_snapshots[eid] = snapshot2;
        entities_snapshots[eid].pop_front();
      }
      else
      {
        float t = (float)(curTime - snapshot1.timestamp) / (snapshot2.timestamp - snapshot1.timestamp);
        auto &e = entities[eid];
        e.x =   (1.f - t) * snapshot1.x   + t * snapshot2.x;
        e.y =   (1.f - t) * snapshot1.y   + t * snapshot2.y;
        e.ori = (1.f - t) * snapshot1.ori + t * snapshot2.ori;
        break;
      }
    }
  }
}

void reduce_inputs(size_t max_inputs_num = 100)
{
  int toDelete = inputs.size() - max_inputs_num;
  if (toDelete > 0)
    inputs.erase(std::begin(inputs), std::begin(inputs) + toDelete);
}

int main(int argc, const char **argv)
{
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

  InitWindow(width, height, "w5 networked MIPT");

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
  uint32_t lastTime = enet_time_get();
  while (!WindowShouldClose())
  {
    ENetEvent event;
    uint32_t curTime = enet_time_get();
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
          on_snapshot(event.packet);
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
      if (entities.find(my_entity) != std::end(entities))
      {
        // Update
        float thr = (up ? 1.f : 0.f) + (down ? -1.f : 0.f);
        float steer = (left ? -1.f : 0.f) + (right ? 1.f : 0.f);

        auto oldTimestamp = entities[my_entity].timestamp;
        auto input = Input{thr, steer, entities[my_entity].timestamp};
        
        simulate_this_entity(input, convert_to_timestamp(curTime - lastTime));
        lastTime += convert_to_timestamp(curTime - lastTime) * timestamp_delta;
        if (oldTimestamp != entities[my_entity].timestamp)
        {
          inputs.push_back(input);
          // Send
          send_entity_input(serverPeer, my_entity, thr, steer);
        }
      }
    }
    interpolate_entities();
    reduce_inputs();
    BeginDrawing();
      ClearBackground(GRAY);
      BeginMode2D(camera);
        for (auto const &[eid, e] : entities)
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
