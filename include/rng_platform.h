#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdio>

#if defined(_WIN32)
  #define NOMINMAX
  #include <windows.h>
  using LibHandle = HMODULE;
  inline LibHandle open_library(const std::string& path) {
    return LoadLibraryA(path.c_str());
  }
  inline void* load_symbol(LibHandle h, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(h, name));
  }
  inline void close_library(LibHandle h) {
    if (h) FreeLibrary(h);
  }
#else
  #include <dlfcn.h>
  using LibHandle = void*;
  inline LibHandle open_library(const std::string& path) {
    return dlopen(path.c_str(), RTLD_NOW);
  }
  inline void* load_symbol(LibHandle h, const char* name) {
    return dlsym(h, name);
  }
  inline void close_library(LibHandle h) {
    if (h) dlclose(h);
  }
#endif

inline unsigned hw_threads() {
  unsigned n = std::thread::hardware_concurrency();
  return n ? n : 1;
}

using clock_type = std::chrono::steady_clock;

struct ScopedTimer {
  clock_type::time_point t0;
  ScopedTimer() : t0(clock_type::now()) {}
  double elapsed_sec() const {
    using namespace std::chrono;
    return duration<double>(clock_type::now() - t0).count();
  }
};
