#pragma once
#include "bitstream.h"
#include "mathUtils.h"
#include <limits>
#include <cassert>

struct float2
{
  float data[2];
  float& operator [](size_t idx) { return data[idx]; }
};

struct float3
{
  float data[3];
  float& operator [](size_t idx) { return data[idx]; }
};

struct Interval
{
  float lo;
  float hi;
};

bool quantized_equal(float x, float y, Interval interval, int num_bits)
{
  int range = (1 << num_bits) - 1;
  float step = (interval.hi - interval.lo) / range;
  return abs(x - y) < step;
}

template<typename T>
T pack_float(float v, Interval interval, int num_bits)
{
  T range = (1 << num_bits) - 1;//std::numeric_limits<T>::max();
  return (T)(range * ((clamp(v, interval.lo, interval.hi) - interval.lo) / (interval.hi - interval.lo)));
}

template<typename T>
float unpack_float(T c, Interval interval, int num_bits)
{
  T range = (1 << num_bits) - 1;//std::numeric_limits<T>::max();
  return float(c) / range * (interval.hi - interval.lo) + interval.lo;
}

template<typename T, int num_bits>
struct PackedFloat
{
  T packedVal;

  PackedFloat(float v, Interval interval) { pack(v, interval); }
  PackedFloat(T compressed_val) : packedVal(compressed_val) {}

  void pack(float v, Interval interval) { packedVal = pack_float<T>(v, interval, num_bits); }
  float unpack(Interval interval) { return unpack_float<T>(packedVal, interval, num_bits); }
};

typedef PackedFloat<uint8_t, 4> float4bitsQuantized;


template<typename T, int num_bits1, int num_bits2>
struct PackedVec2
{
  T packedVal;
  PackedVec2(float v1, float v2, Interval interval) { pack(v1, v2, interval); }
  PackedVec2(float2 v, Interval interval) { pack(v, interval); }
  PackedVec2(T compressed_val) : packedVal(compressed_val) {}

  void pack(float v1, float v2, Interval interval)
  {
    const auto packedVal1 = pack_float<T>(v1, interval, num_bits1);
    const auto packedVal2 = pack_float<T>(v2, interval, num_bits2);
    packedVal = packedVal1 << num_bits2 | packedVal2;
  }

  void pack(float2 v, Interval interval) { pack(v[0], v[1], interval); }

  float2 unpack(Interval interval)
  {
    float2 val;
    val[0] = unpack_float<T>(packedVal >> num_bits2, interval, num_bits1);
    val[1] = unpack_float<T>(packedVal & ((1 << num_bits2) - 1), interval, num_bits2);
    return val;
  }
};

template<typename T, int num_bits1, int num_bits2, int num_bits3>
struct PackedVec3
{
  T packedVal;
  PackedVec3(float v1, float v2, float v3, Interval interval) { pack(v1, v2, v3, interval); }
  PackedVec3(float3 v, Interval interval) { pack(v, interval); }
  PackedVec3(T compressed_val) : packedVal(compressed_val) {}

  void pack(float v1, float v2, float v3, Interval interval)
  {
    const auto packedVal1 = pack_float<T>(v1, interval, num_bits1);
    const auto packedVal2 = pack_float<T>(v2, interval, num_bits2);
    const auto packedVal3 = pack_float<T>(v3, interval, num_bits3);
    packedVal = (packedVal1 << (num_bits2 + num_bits3)) | (packedVal2 << num_bits3) | packedVal3;
  }

  void pack(float3 v, Interval interval) { pack(v[0], v[1], v[2], interval); }

  float3 unpack(Interval interval)
  {
    float3 val;
    val[0] = unpack_float<T>(packedVal >> (num_bits2 + num_bits3), interval, num_bits1);
    val[1] = unpack_float<T>((packedVal >> num_bits3) & ((1 << num_bits2) - 1), interval, num_bits2);
    val[2] = unpack_float<T>(packedVal & ((1 << num_bits3) - 1), interval, num_bits3);
    return val;
  }
};

void hw_test()
{
  Interval interval = {-1.f, 1.f};
  float testFloat = 0.123f;
  PackedFloat<uint8_t, 4> floatPacked(testFloat, interval);
  auto unpackedFloat = floatPacked.unpack(interval);
  assert(quantized_equal(unpackedFloat, testFloat, interval, 4));

  interval = {-10.f, 10.f};
  float2 testFloat2 = {4.f, -5.f};
  PackedVec2<uint16_t, 8, 8> packed_vec2(testFloat2, interval);
  auto unpackedFloat2 = packed_vec2.unpack(interval);
  assert(quantized_equal(unpackedFloat2[0], testFloat2[0], interval, 8));
  assert(quantized_equal(unpackedFloat2[1], testFloat2[1], interval, 8));

  interval = {-1.f, 1.f};
  float3 testFloat3 = {0.1f, -0.2f, 0.3f};
  PackedVec3<uint32_t, 11, 10, 11> packed_vec3(testFloat3, interval);
  auto unpackedFloat3 = packed_vec3.unpack(interval);
  assert(quantized_equal(unpackedFloat3[0], testFloat3[0], interval, 11));
  assert(quantized_equal(unpackedFloat3[1], testFloat3[1], interval, 10));
  assert(quantized_equal(unpackedFloat3[2], testFloat3[2], interval, 11));

  uint32_t values_to_check[] = {123, 4567, 20000, 2000000000};

  uint8_t memory_for_bs[1024];
  Bitstream write_bs = Bitstream(memory_for_bs);
  Bitstream read_bs  = Bitstream(memory_for_bs);
  for (size_t i = 0; i < 4; ++i)
    write_bs.packed_int32(values_to_check[i]);

  for (size_t i = 0; i < 3; ++i)
    assert(read_bs.unpacked_int32() == values_to_check[i]);
}