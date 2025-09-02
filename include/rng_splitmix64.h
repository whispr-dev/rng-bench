#pragma once
#include <cstdint>

// splitmix64 – good seeding utility (public domain)
struct splitmix64 {
  uint64_t state;
  explicit splitmix64(uint64_t s) : state(s) {}
  uint64_t next() {
    uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
};
