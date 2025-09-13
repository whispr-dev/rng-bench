#pragma once
#include "rng_platform.h"
#include <cstdint>
#include <string>
#include <stdexcept>

// Dynamic adapter for your C-SIMD-RNG-Lib, using the C API from the repo README:
// universal_rng_new(seed, algo_id, bitwidth)
// universal_rng_next_u64(rng*)
// universal_rng_next_double(rng*)
// universal_rng_free(rng*)
//
// On success, you get per-thread state instances and direct function pointers.

struct CSimdLib {
  LibHandle lib{};
  using new_fn = void* (*)(uint64_t, int, int);
  using next_u64_fn = uint64_t (*)(void*);
  using next_double_fn = double (*)(void*);
  using free_fn = void (*)(void*);

  new_fn p_new{};
  next_u64_fn p_next_u64{};
  next_double_fn p_next_double{};
  free_fn p_free{};

  explicit CSimdLib(const std::string& libpath) {
    lib = open_library(libpath);
    if (!lib) throw std::runtime_error("failed to open C-SIMD-RNG library at: " + libpath);

    p_new  = reinterpret_cast<new_fn>(load_symbol(lib, "universal_rng_new"));
    p_next_u64 = reinterpret_cast<next_u64_fn>(load_symbol(lib, "universal_rng_next_u64"));
    p_next_double = reinterpret_cast<next_double_fn>(load_symbol(lib, "universal_rng_next_double"));
    p_free = reinterpret_cast<free_fn>(load_symbol(lib, "universal_rng_free"));

    if (!p_new || !p_next_u64 || !p_next_double || !p_free) {
      close_library(lib);
      throw std::runtime_error("failed to resolve required symbols in C-SIMD-RNG lib");
    }
  }

  ~CSimdLib() { close_library(lib); }

  struct Instance {
    CSimdLib* owner{};
    void* state{};
    Instance() = default;
    Instance(CSimdLib* o, uint64_t seed, int algo_id, int bitwidth) : owner(o) {
      state = owner->p_new(seed, algo_id, bitwidth);
      if (!state) throw std::runtime_error("universal_rng_new returned null");
    }
    ~Instance() {
      if (owner && state) owner->p_free(state);
    }
    inline uint64_t next_u64() { return owner->p_next_u64(state); }
    inline double   next_double() { return owner->p_next_double(state); }
  };
};
