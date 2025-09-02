#pragma once
#include <cstdint>
#include <array>
#include <cmath>
#include <algorithm>

struct RunningStats {
  // Welford's algorithm
  long double mean = 0.0L;
  long double m2   = 0.0L;
  uint64_t n = 0;

  void push(double x) {
    ++n;
    long double d  = x - mean;
    mean += d / static_cast<long double>(n);
    long double d2 = x - mean;
    m2 += d * d2;
  }
  double variance() const {
    return (n > 1) ? static_cast<double>(m2 / static_cast<long double>(n - 1)) : 0.0;
  }
};

struct ByteHist {
  std::array<uint64_t,256> bins{};
  void push_u64(uint64_t x) {
    // 8 bytes
    for (int i=0;i<8;++i) {
      bins[ static_cast<unsigned>((x >> (i*8)) & 0xFFu) ]++;
    }
  }
  // Chi-square vs uniform over 256, using total observed samples (bytes)
  double chi_square() const {
    uint64_t total = 0;
    for (auto v: bins) total += v;
    if (!total) return 0.0;
    long double expct = static_cast<long double>(total) / 256.0L;
    long double chi = 0.0L;
    for (auto v: bins) {
      long double diff = static_cast<long double>(v) - expct;
      chi += (diff * diff) / expct;
    }
    return static_cast<double>(chi);
  }
};
