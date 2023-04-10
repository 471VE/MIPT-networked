#pragma once
#include <numeric>

struct Snapshot
{
  float x;
  float y;
  float ori;
  uint32_t timestamp;
};

struct Input
{
  float thr;
  float steer;
  uint32_t timestamp;
};

const uint32_t timestamp_delta = 17;
const float dt = timestamp_delta * 0.001f;

uint32_t convert_to_timestamp(uint32_t time) { return time / timestamp_delta; }

