#pragma once

#include <cstddef>
#include <cstring>

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

  private:
    uint8_t* dataPtr;
    uint32_t dataOffset;
};