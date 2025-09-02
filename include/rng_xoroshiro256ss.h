#pragma once
#include <cstdint>

// xoshiro256** 1.0 – Public domain by Blackman & Vigna
// https://prng.di.unimi.it/
struct xoshiro256ss {
  uint64_t s[4];

  static inline uint64_t rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
  }
  explicit xoshiro256ss(uint64_t seed) {
    // seed via splitmix64 chain
    uint64_t z = seed;
    auto sm = [&z](){
      z += 0x9E3779B97F4A7C15ull;
      uint64_t t = z;
      t = (t ^ (t >> 30)) * 0xBF58476D1CE4E5B9ull;
      t = (t ^ (t >> 27)) * 0x94D049BB133111EBull;
      return t ^ (t >> 31);
    };
    for (int i=0;i<4;++i) s[i] = sm();
  }
  inline uint64_t next_u64() {
    const uint64_t result = rotl(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = rotl(s[3], 45);

    return result;
  }
  inline double next_double() {
    return (next_u64() >> 11) * (1.0/9007199254740992.0);
  }
};
