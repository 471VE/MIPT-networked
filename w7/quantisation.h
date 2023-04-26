#pragma once
#include "mathUtils.h"
#include <limits>
#include <cassert>
#include <iostream>

const int i = 1;
#define is_bigendian() ( (*(char*)&i) == 0 )

uint16_t reverse_byte_order_if_little_endian(uint16_t i) {
  uint8_t c1, c2;    
  if (is_bigendian())
    return i;
  else
  {
    c1 = i & 255;
    c2 = (i >> 8) & 255;
    return (c1 << 8) + c2;
  }
}

uint32_t reverse_byte_order_if_little_endian(uint32_t i) {
  uint8_t c1, c2, c3, c4;
  if (is_bigendian())
    return i;
  else
  {
    c1 = i & 255;
    c2 = (i >> 8) & 255;
    c3 = (i >> 16) & 255;
    c4 = (i >> 24) & 255;
    return ((uint32_t)c1 << 24) + ((uint32_t)c2 << 16) + ((uint32_t)c3 << 8) + c4;
  }
}

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
  return range * ((clamp(v, interval.lo, interval.hi) - interval.lo) / (interval.hi - interval.lo));
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

static uint32_t uint8_limit = std::numeric_limits<uint8_t>::max();
static uint32_t uint16_limit = std::numeric_limits<uint16_t>::max();
static uint32_t uint32_limit = std::numeric_limits<uint32_t>::max();

// void* is used in order to recreate the conditions when we have a continuous stream of data and we don't know where there is
// the end of packed data. For these purposes byte order must be big-endian.
void* packed_int32(uint32_t val)
{
  assert(val >= 0);
  if (val < uint8_limit / 2)
  {
    uint8_t* ptr = new uint8_t;
    *ptr = val;
    std::cout << "Packed int size: " << sizeof(*ptr) << " bytes\n";
    return ptr;
  }
  else if (val < uint16_limit / 4)
  {
    uint16_t* ptr = new uint16_t;
    uint16_t data = (1 << 15) | val;
    *ptr = reverse_byte_order_if_little_endian(data);
    std::cout << "Packed int size: " << sizeof(*ptr) << " bytes\n";
    return ptr;
  }
  else if (val < uint32_limit / 4)
  {
    uint32_t* ptr = new uint32_t;
    uint32_t data = (3 << 30) | val;
    *ptr = reverse_byte_order_if_little_endian(data);
    std::cout << "Packed int size: " << sizeof(*ptr) << " bytes\n";
    return ptr;
  }
  else
  {
    std::cout << "Unsupported value\n";
    return nullptr;
  }
}

uint32_t unpacked_int32(void* ptr)
{
  if (!ptr)
  {
    std::cerr << "Fatal error: data is nullptr. Most likely the value that was packed exceeded upper limit of " << uint32_limit / 4 << "\n";
    exit(1);
  }
  uint8_t first_byte = *(uint8_t*)ptr;
  uint8_t type = first_byte >> 6;
  if (type < 2)
    return (uint32_t)first_byte;
  else if (type == 2)
    return (uint32_t)(reverse_byte_order_if_little_endian(*(uint16_t*)ptr) & 0x3fff);
  else
    return (uint32_t)(reverse_byte_order_if_little_endian(*(uint32_t*)ptr) & 0x3fffffff);
}

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
  void* packed_num1 = packed_int32(values_to_check[0]); // 1 byte
  assert(unpacked_int32(packed_num1) == values_to_check[0]);

  void* packed_num2 = packed_int32(values_to_check[1]); // 2 bytes
  assert(unpacked_int32(packed_num2) == values_to_check[1]);

  void* packed_num3 = packed_int32(values_to_check[2]); // 4 bytes
  assert(unpacked_int32(packed_num3) == values_to_check[2]);

  void* packed_num4 = packed_int32(values_to_check[3]); // Unsupported
  assert(packed_num4 == nullptr);
}