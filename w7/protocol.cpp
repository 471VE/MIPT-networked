#include "auxilary.h"
#include "protocol.h"
#include "quantisation.h"
#include <cstring> // memcpy
#include <iostream>

void send_join(ENetPeer *peer)
{
  ENetPacket *packet = enet_packet_create(nullptr, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
  *packet->data = E_CLIENT_TO_SERVER_JOIN;

  Snapshot snapshot;
  clientSnapshots.snapshotHistory.push_back(snapshot);

  enet_peer_send(peer, 0, packet);
}

void send_new_entity(ENetPeer *peer, const Entity &ent)
{
  ENetPacket *packet = enet_packet_create(nullptr, sizeof(uint8_t) + sizeof(Entity),
                                                   ENET_PACKET_FLAG_RELIABLE);
  uint8_t *ptr = packet->data;
  *ptr = E_SERVER_TO_CLIENT_NEW_ENTITY; ptr += sizeof(uint8_t);
  memcpy(ptr, &ent, sizeof(Entity)); ptr += sizeof(Entity);

  enet_peer_send(peer, 0, packet);
}

void send_set_controlled_entity(ENetPeer *peer, uint16_t eid)
{
  ENetPacket *packet = enet_packet_create(nullptr, sizeof(uint8_t) + sizeof(uint16_t),
                                                   ENET_PACKET_FLAG_RELIABLE);
  uint8_t *ptr = packet->data;
  *ptr = E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY; ptr += sizeof(uint8_t);
  memcpy(ptr, &eid, sizeof(uint16_t)); ptr += sizeof(uint16_t);

  enet_peer_send(peer, 0, packet);

  serverInputHistory.emplace(eid, std::deque<Input>());
}

void send_entity_input(ENetPeer* peer, uint16_t eid, float thr, float ori, uint8_t header, uint16_t current_input_id, uint16_t reference_input_id)
{
  size_t packet_size = sizeof(uint8_t) + sizeof(uint16_t) * 3 + sizeof(uint8_t);
  if (header == 128)
    packet_size += sizeof(uint8_t);

  ENetPacket* packet = enet_packet_create(nullptr, packet_size, ENET_PACKET_FLAG_UNSEQUENCED);
  Bitstream bs(packet->data);
  bs.write(E_CLIENT_TO_SERVER_INPUT);
  bs.write(eid);
  bs.write(current_input_id);
  bs.write(reference_input_id);
  bs.write(header);

  if (header == 128)
  {
    float4bitsQuantized thrPacked(thr, {-1.f, 1.f});
    float4bitsQuantized oriPacked(ori, {-1.f, 1.f});
    uint8_t thrSteerPacked = (thrPacked.packedVal << 4) | oriPacked.packedVal;
    bs.write(thrSteerPacked);
  }
  /*
  memcpy(ptr, &thrPacked, sizeof(uint8_t)); ptr += sizeof(uint8_t);
  memcpy(ptr, &oriPacked, sizeof(uint8_t)); ptr += sizeof(uint8_t);
  */

  enet_peer_send(peer, 1, packet);
}

typedef PackedFloat<uint16_t, 11> PositionXQuantized;
typedef PackedFloat<uint16_t, 10> PositionYQuantized;

void send_snapshot(ENetPeer* peer, std::vector<Entity>& entities, std::map<uint16_t, uint8_t> headers, uint16_t current_snapshot_id,
  uint16_t reference_snapshot_id)
{
  size_t packet_size = sizeof(uint8_t) + sizeof(size_t) + headers.size() * sizeof(uint8_t) + (entities.size() + 2) * sizeof(uint16_t);
  for (Entity& ent : entities)
  {
    if (headers[ent.eid] != 0)
    {
      if ((headers[ent.eid] & 0b01000000) == 0b01000000)
        packet_size += 2 * sizeof(uint16_t);
      if ((headers[ent.eid] & 0b00100000) == 0b00100000)
        packet_size += sizeof(uint8_t);
    }
  }

  ENetPacket* packet = enet_packet_create(nullptr, packet_size, ENET_PACKET_FLAG_UNSEQUENCED);
  Bitstream bs(packet->data);
  bs.write(E_SERVER_TO_CLIENT_SNAPSHOT);
  bs.write(entities.size());
  bs.write(current_snapshot_id);
  bs.write(reference_snapshot_id);
  for (Entity& ent : entities)
  {
    bs.write(ent.eid);
    bs.write(headers[ent.eid]);
    if (headers[ent.eid] != 0)
    {
      if ((headers[ent.eid] & 0b01000000) == 0b01000000)
      {
        PositionXQuantized xPacked(ent.x, {-16, 16});
        PositionYQuantized yPacked(ent.y, {-8, 8});
        bs.write(xPacked.packedVal);
        bs.write(yPacked.packedVal);
      }
      if ((headers[ent.eid] & 0b00100000) == 0b00100000)
      {
        uint8_t oriPacked = pack_float<uint8_t>(ent.ori, {-PI, PI}, 8);
        bs.write(oriPacked);
      }
    }       
  }
  enet_peer_send(peer, 1, packet);
}

void send_input_acknowledgement(ENetPeer* peer, uint16_t reference_input_id)
{
  ENetPacket* packet = enet_packet_create(nullptr, sizeof(uint8_t) + sizeof(uint16_t), ENET_PACKET_FLAG_UNSEQUENCED);
  Bitstream bs(packet->data);
  bs.write(E_SERVER_TO_CLIENT_INPUT_ACK);
  bs.write(reference_input_id);
  enet_peer_send(peer, 1, packet);
}

void send_snapshot_acknowledgement(ENetPeer* peer, uint16_t eid, uint16_t reference_snapshot_id)
{
  ENetPacket* packet = enet_packet_create(nullptr, sizeof(uint8_t) + sizeof(uint16_t) * 2, ENET_PACKET_FLAG_UNSEQUENCED);
  Bitstream bs(packet->data);
  bs.write(E_CLIENT_TO_SERVER_SNAPSHOT_ACK);
  bs.write(eid);
  bs.write(reference_snapshot_id);
  enet_peer_send(peer, 1, packet);
}

MessageType get_packet_type(ENetPacket *packet)
{
  return (MessageType)*packet->data;
}

void deserialize_new_entity(ENetPacket *packet, Entity &ent)
{
  uint8_t *ptr = packet->data; ptr += sizeof(uint8_t);
  ent = *(Entity*)(ptr); ptr += sizeof(Entity);
}

void deserialize_set_controlled_entity(ENetPacket *packet, uint16_t &eid)
{
  uint8_t *ptr = packet->data; ptr += sizeof(uint8_t);
  eid = *(uint16_t*)(ptr); ptr += sizeof(uint16_t);
}

void deserialize_entity_input(ENetPacket* packet, uint16_t& eid, float& thr, float& steer, uint16_t current_input_id)
{
  uint16_t reference_input_id;
  uint8_t header;
  Bitstream bs(packet->data);
  MessageType messageType;
  bs.read(messageType);
  bs.read(eid);
  bs.read(current_input_id);
  bs.read(reference_input_id);
  bs.read(header);

  if (header == 128)
  {
    uint8_t thrSteerPacked;
    bs.read(thrSteerPacked);
    uint8_t neutralPackedValue = pack_float<uint8_t>(0.f, {-1.f, 1.f}, 4);
    uint8_t nominalPackedValue = pack_float<uint8_t>(1.f, {0.f, 1.2f}, 4);
    float4bitsQuantized thrPacked(thrSteerPacked >> 4);
    float4bitsQuantized steerPacked(thrSteerPacked & 0x0f);
    thr = thrPacked.packedVal == neutralPackedValue ? 0.f : thrPacked.unpack({-1.f, 1.f});
    steer = steerPacked.packedVal == neutralPackedValue ? 0.f : steerPacked.unpack({-1.f, 1.f});
  }
  else
    for (Input& input : serverInputHistory[eid].inputHistory)
      if (input.id == reference_input_id)
      {
        steer = input.steer;
        thr = input.thr;
      }

  Input new_input;
  new_input.id = current_input_id;
  new_input.steer = steer;
  new_input.thr = thr;

  serverInputHistory[eid].inputHistory.push_back(new_input);
  if (serverInputHistory[eid].reference_id < reference_input_id)
  {
    while (serverInputHistory[eid].inputHistory[0].id != reference_input_id)
      serverInputHistory[eid].inputHistory.pop_front();
    serverInputHistory[eid].reference_id = reference_input_id;
  }
}

void deserialize_snapshot(ENetPacket* packet, std::vector<Entity>& entities, uint16_t& current_snapshot_id)
{
  Bitstream bs(packet->data);
  MessageType messageType;
  bs.read(messageType);
  size_t entities_num;
  bs.read(entities_num);
  uint8_t header;
  uint16_t reference_snapshot_id;
  bs.read(current_snapshot_id);
  bs.read(reference_snapshot_id);
  Snapshot snapshot;
  snapshot.id = current_snapshot_id;
  for (size_t i = 0; i < entities_num; ++i)
  {
    uint16_t eid;
    bs.read(eid);
    bs.read(header);

    for (Entity& ent : entities)
      if (ent.eid == eid)
      {
        if (header != 0)
        {
          if ((header & 0b01000000) == 0b01000000)
          {
            uint16_t xPacked, yPacked;
            bs.read(xPacked);
            bs.read(yPacked);
            PositionXQuantized xPackedVal(xPacked);
            PositionYQuantized yPackedVal(yPacked);
            ent.x = xPackedVal.unpack({-16, 16});
            ent.y = yPackedVal.unpack({-8, 8});
          }
          else
          {
            for (Snapshot& snapshot2 : clientSnapshots.snapshotHistory)
              if (snapshot2.id == reference_snapshot_id)
              {
                for (Entity& e : snapshot2.entities)
                  if (ent.eid == e.eid)
                  {
                    ent.x = e.x;
                    ent.y = e.y;
                    break;
                  }
                break;
              }
          }

          if ((header & 0b00100000) == 0b00100000)
          {
            uint8_t oriPacked;
            bs.read(oriPacked);
            ent.ori = unpack_float<uint8_t>(oriPacked, {-PI, PI}, 8);
          }
          else
          {
            for (Snapshot& snapshot2 : clientSnapshots.snapshotHistory)
              if (snapshot2.id == reference_snapshot_id)
              {
                for (Entity& e : snapshot2.entities)
                  if (ent.eid == e.eid)
                  {
                    ent.ori = e.ori;
                    break;
                  }
                break;
              }
          }
        }
        else
        {
          for (Snapshot& snapshot2 : clientSnapshots.snapshotHistory)
            if (snapshot2.id == reference_snapshot_id)
            {
              for (Entity& e : snapshot2.entities)
                if (ent.eid == e.eid)
                {
                  ent.x = e.x;
                  ent.y = e.y;
                  ent.ori = e.ori;
                  break;
                }
              break;
            }
        }
        snapshot.entities.push_back(ent);
      }
  }

  clientSnapshots.snapshotHistory.push_back(snapshot);
  if (clientSnapshots.reference_id < reference_snapshot_id)
  {
    while (clientSnapshots.snapshotHistory[0].id != reference_snapshot_id)
      clientSnapshots.snapshotHistory.pop_front();
    clientSnapshots.reference_id = reference_snapshot_id;
  }
}


void deserialize_input_acknowledgement(ENetPacket* packet, uint16_t& reference_input_id)
{
  Bitstream bs(packet->data);
  MessageType messageType;
  bs.read(messageType);
  bs.read(reference_input_id);
}

void deserialize_snapshot_acknowledgement(ENetPacket* packet, uint16_t& eid, uint16_t& reference_snapshot_id)
{
  Bitstream bs(packet->data);
  MessageType messageType;
  bs.read(messageType);
  bs.read(eid);
  bs.read(reference_snapshot_id);
}