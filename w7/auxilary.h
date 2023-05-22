#pragma once

#include <deque>
#include <vector>

#include "entity.h"

struct Snapshot
{
	std::vector<Entity> entities;
	uint16_t id = 0;
};

struct SnapshotHistory
{
  std::deque<Snapshot> snapshotHistory;
  uint16_t reference_id = 0;
};

struct Input
{
  float thr;
  float steer;
  uint16_t id;
};

struct InputHistory
{
	std::deque<Input> inputHistory;
	uint16_t reference_id = 0;
};