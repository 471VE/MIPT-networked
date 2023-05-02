#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>

const int i = 1;
#define is_bigendian() ( (*(char*)&i) == 0 )

static uint16_t reverse_byte_order_if_little_endian(uint16_t i) {
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

static uint32_t reverse_byte_order_if_little_endian(uint32_t i) {
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

static uint32_t uint8_limit = std::numeric_limits<uint8_t>::max();
static uint32_t uint16_limit = std::numeric_limits<uint16_t>::max();
static uint32_t uint32_limit = std::numeric_limits<uint32_t>::max();

class Bitstream {
  public:
    Bitstream(uint8_t* data)
    : dataPtr(data)
    , dataOffset(0)
    {}

    template<typename Type>
    void write(const Type& val) {
      memcpy(dataPtr + dataOffset, reinterpret_cast<const uint8_t*>(&val), sizeof(Type));
      dataOffset += sizeof(Type);
    }

    template<typename Type>
    void read(Type& val) {
      memcpy(reinterpret_cast<uint8_t*>(&val), dataPtr + dataOffset, sizeof(Type));
      dataOffset += sizeof(Type);
    }

  void packed_int32(uint32_t val)
  {
    assert(val >= 0);
    if (val >= uint32_limit / 4)
    {
      #ifdef _DEBUG
        std::cerr << "Unsupported value\n";
      #endif
      return;
    }
    size_t packedSize = 0;
    if (val < uint8_limit / 2)
    {
      uint8_t* ptr = new uint8_t;
      *ptr = val;
      packedSize = sizeof(*ptr);
      memcpy(dataPtr + dataOffset, ptr, packedSize);
    }
    else if (val < uint16_limit / 4)
    {
      uint16_t* ptr = new uint16_t;
      uint16_t data = (1 << 15) | val;
      *ptr = reverse_byte_order_if_little_endian(data);
      packedSize = sizeof(*ptr);
      memcpy(dataPtr + dataOffset, ptr, packedSize);
    }
    else
    {
      uint32_t* ptr = new uint32_t;
      uint32_t data = (3 << 30) | val;
      *ptr = reverse_byte_order_if_little_endian(data);
      packedSize = sizeof(*ptr);
      memcpy(dataPtr + dataOffset, ptr, packedSize);
    }
    #ifdef _DEBUG
      std::cout << "Packed int size: " << packedSize << " bytes\n";
    #endif
    dataOffset += packedSize;
  }

  uint32_t unpacked_int32()
  {
    uint8_t first_byte = *(uint8_t*)(dataPtr + dataOffset);
    uint8_t type = first_byte >> 6;
    uint32_t return_val = 0;
    if (type < 2)
    {
      return_val = (uint32_t)first_byte;
      dataOffset += sizeof(uint8_t);
    }
    else if (type == 2)
    {
      return_val = (uint32_t)(reverse_byte_order_if_little_endian(*(uint16_t*)(dataPtr + dataOffset)) & 0x3fff);
      dataOffset += sizeof(uint16_t);
    }
    else
    {
      return_val = (uint32_t)(reverse_byte_order_if_little_endian(*(uint32_t*)(dataPtr + dataOffset)) & 0x3fffffff);
      dataOffset += sizeof(uint32_t);
    }
    #ifdef _DEBUG
     std::cout << "Unpacked value: " << return_val << std::endl;
    #endif
    return return_val;
  }

  private:
    uint8_t* dataPtr;
    uint32_t dataOffset;
};