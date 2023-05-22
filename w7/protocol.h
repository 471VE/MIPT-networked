#pragma once
#include <enet/enet.h>
#include <cstdint>
#include "entity.h"
#include <vector>
#include <map>

enum MessageType : uint8_t
{
  E_CLIENT_TO_SERVER_JOIN = 0,
  E_SERVER_TO_CLIENT_NEW_ENTITY,
  E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY,
  E_CLIENT_TO_SERVER_INPUT,
  E_SERVER_TO_CLIENT_SNAPSHOT,
	E_SERVER_TO_CLIENT_INPUT_ACK,
	E_CLIENT_TO_SERVER_SNAPSHOT_ACK,
};

struct InputHistory;
struct SnapshotHistory;

static std::map<uint16_t, InputHistory> serverInputHistory;
static SnapshotHistory clientSnapshots;

void send_join(ENetPeer *peer);
void send_new_entity(ENetPeer *peer, const Entity &ent);
void send_set_controlled_entity(ENetPeer *peer, uint16_t eid);
void send_entity_input(ENetPeer* peer, uint16_t eid, float thr, float steer, uint8_t header, uint16_t current_input_id, uint16_t reference_input_id);
void send_snapshot(ENetPeer* peer, std::vector<Entity>& entities, std::map<uint16_t, uint8_t> headers, uint16_t current_snapshot_id,
  uint16_t reference_snapshot_id);
void send_input_acknowledgement(ENetPeer* peer, uint16_t reference_input_id);
void send_snapshot_acknowledgement(ENetPeer* peer, uint16_t eid, uint16_t reference_snapshot_id);

MessageType get_packet_type(ENetPacket *packet);

void deserialize_new_entity(ENetPacket *packet, Entity &ent);
void deserialize_set_controlled_entity(ENetPacket *packet, uint16_t &eid);
void deserialize_entity_input(ENetPacket* packet, uint16_t& eid, float& thr, float& steer, uint16_t current_input_id);
void deserialize_snapshot(ENetPacket* packet, std::vector<Entity>& entities, uint16_t& current_snapshot_id);
void deserialize_input_acknowledgement(ENetPacket* packet, uint16_t& reference_input_id);
void deserialize_snapshot_acknowledgement(ENetPacket* packet, uint16_t& eid, uint16_t& reference_snapshot_id);

void hw_test();