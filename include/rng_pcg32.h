#pragma once
#include <cstdint>

// PCG32 (XSH-RR), minimal implementation – public domain style API
// Reference: Melissa E. O'Neill, "PCG: A Family of Simple Fast Space-Efficient Statistically Good PRNGs"
struct pcg32 {
  uint64_t state;
  uint64_t inc; // must be odd

  pcg32(uint64_t seed, uint64_t seq = 54u) {
    state = 0u;
    inc   = (seq << 1u) | 1u;
    next_u32();
    state += seed;
    next_u32();
  }

  inline uint32_t next_u32() {
    uint64_t oldstate = state;
    state = oldstate * 6364136223846793005ULL + inc;
    uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = static_cast<uint32_t>(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-static_cast<int>(rot)) & 31));
  }

  inline uint64_t next_u64() {
    uint64_t a = next_u32();
    uint64_t b = next_u32();
    return (a << 32) | b;
  }

  inline double next_double() {
    // 53-bit mantissa from 64-bit integer
    return (next_u64() >> 11) * (1.0/9007199254740992.0);
  }
};
