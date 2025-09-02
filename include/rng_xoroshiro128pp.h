#pragma once
#include <cstdint>

// xoroshiro128++ 1.0 – Public domain by Blackman & Vigna
// https://prng.di.unimi.it/
struct xoroshiro128pp {
  uint64_t s0, s1;
  static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
  }
  explicit xoroshiro128pp(uint64_t seed) {
    // seed via splitmix64 chain
    uint64_t z = seed;
    auto sm = [&z](){
      z += 0x9E3779B97F4A7C15ull;
      uint64_t t = z;
      t = (t ^ (t >> 30)) * 0xBF58476D1CE4E5B9ull;
      t = (t ^ (t >> 27)) * 0x94D049BB133111EBull;
      return t ^ (t >> 31);
    };
    s0 = sm(); s1 = sm();
  }
  inline uint64_t next_u64() {
    uint64_t r = rotl(s0 + s1, 17) + s0;
    s1 ^= s0;
    s0 = rotl(s0, 49) ^ s1 ^ (s1 << 21);
    s1 = rotl(s1, 28);
    return r;
  }
  inline double next_double() {
    return (next_u64() >> 11) * (1.0/9007199254740992.0);
  }
};
