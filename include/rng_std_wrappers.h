#pragma once
#include <random>
#include <cstdint>

struct std_mt19937 {
  std::mt19937 gen;
  explicit std_mt19937(uint64_t seed) : gen(static_cast<uint32_t>(seed)) {}
  inline uint64_t next_u64() {
    uint64_t a = gen(), b = gen();
    return (a << 32) | b;
  }
  inline double next_double() {
    // use 53 bits from two 32-bit draws
    uint64_t x = next_u64() >> 11;
    return x * (1.0/9007199254740992.0);
  }
};

struct std_mt19937_64 {
  std::mt19937_64 gen;
  explicit std_mt19937_64(uint64_t seed) : gen(seed) {}
  inline uint64_t next_u64() { return gen(); }
  inline double next_double() {
    return (next_u64() >> 11) * (1.0/9007199254740992.0);
  }
};

struct std_minstd_rand {
  std::minstd_rand gen;
  explicit std_minstd_rand(uint64_t seed) : gen(static_cast<uint32_t>(seed)) {}
  inline uint64_t next_u64() {
    uint64_t a = gen(), b = gen();
    return (a << 32) | b;
  }
  inline double next_double() {
    uint64_t x = next_u64() >> 11;
    return x * (1.0/9007199254740992.0);
  }
};

struct std_ranlux48 {
  std::ranlux48 gen;
  explicit std_ranlux48(uint64_t seed) : gen(static_cast<unsigned long>(seed)) {}
  inline uint64_t next_u64() {
    uint64_t a = gen(), b = gen();
    return (a << 32) | b;
  }
  inline double next_double() {
    uint64_t x = next_u64() >> 11;
    return x * (1.0/9007199254740992.0);
  }
};
