#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <optional>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "rng_platform.h"
#include "rng_stats.h"
#include "rng_csv.h"

#include "rng_splitmix64.h"
#include "rng_pcg32.h"
#include "rng_xoroshiro128pp.h"
// #include "rng_xoshiro256ss.h"
#include "rng_std_wrappers.h"
#include "rng_csimd_dynamic.h"

struct BenchResult {
  std::string name;
  uint64_t total_u64 = 0;
  double secs_u64 = 0.0;
  double ops_per_s_u64 = 0.0;

  uint64_t total_f64 = 0;
  double secs_f64 = 0.0;
  double ops_per_s_f64 = 0.0;

  double mean_f64 = 0.0;
  double var_f64  = 0.0;
  double chi2_bytes = 0.0;
  unsigned threads = 1;
};

struct Cmd {
  uint64_t total = 100000000ULL; // total samples per generator
  unsigned threads = hw_threads();
  std::string csv_path;
  std::string csimd_path; // path to your lib(.so/.dll/.dylib); if empty, skip
  int csimd_algo = 0;     // e.g. 0 for xoroshiro128++, per your lib's mapping
  int csimd_bitwidth = 1; // 1 = 64-bit
  uint64_t seed = 0xC0FFEED5EEDULL;
  std::vector<std::string> gens; // if empty -> all
};

static void usage(const char* argv0) {
  std::printf(
R"(usage: %s [options]

options:
  --total N             total samples per generator (default 100000000)
  --threads T           number of threads (default: hardware_concurrency)
  --seed S              base seed (u64, default 0xC0FFEED5EED)
  --csv PATH            write results to CSV at PATH
  --gens LIST           comma-separated list: std_mt19937,std_mt19937_64,std_minstd,ranlux48,
                        xoroshiro128pp,xoshiro256ss,pcg32,csimd
  --csimd-lib PATH      path to your C-SIMD-RNG shared lib (dll/so/dylib)
  --csimd-algo ID       algorithm id to pass to universal_rng_new (default 0)
  --csimd-bw   BW       bitwidth to pass (1=64-bit) (default 1)
  --help

examples:
  Linux/macOS:
    ./rng_bench --total 200000000 --threads 8 --csimd-lib /home/wofl/C-SIMD-RNG-Lib/lib_files/linux_shared/libuniversal_rng.so

  Windows (PowerShell):
    .\build\rng_bench.exe --total 200000000 --threads 8 --csimd-lib "C:\GitHub\C-SIMD-RNG-Lib\lib_files\mingw_shared\universal_rng.dll"
)", argv0);
}

static Cmd parse(int argc, char** argv) {
  Cmd c;
  for (int i=1;i<argc;++i) {
    std::string a = argv[i];
    auto need = [&](int more){ if (i+more>=argc) { usage(argv[0]); std::exit(1);} };
    if (a=="--help" || a=="-h") { usage(argv[0]); std::exit(0); }
    else if (a=="--total") { need(1); c.total = std::stoull(argv[++i]); }
    else if (a=="--threads") { need(1); c.threads = (unsigned)std::stoul(argv[++i]); if (c.threads==0) c.threads=1; }
    else if (a=="--seed") { need(1); std::stringstream ss; ss<<std::hex<<argv[++i]; ss>>c.seed; if(!ss) c.seed = std::stoull(argv[i]); }
    else if (a=="--csv") { need(1); c.csv_path = argv[++i]; }
    else if (a=="--gens") { need(1); 
      std::string s = argv[++i];
      size_t pos=0; 
      while (true) {
        size_t comma = s.find(',', pos);
        c.gens.emplace_back(s.substr(pos, comma==std::string::npos ? s.size()-pos : comma-pos));
        if (comma==std::string::npos) break; pos = comma+1;
      }
    }
    else if (a=="--csimd-lib") { need(1); c.csimd_path = argv[++i]; }
    else if (a=="--csimd-algo") { need(1); c.csimd_algo = std::stoi(argv[++i]); }
    else if (a=="--csimd-bw") { need(1); c.csimd_bitwidth = std::stoi(argv[++i]); }
    else { std::fprintf(stderr, "unknown option: %s\n", a.c_str()); usage(argv[0]); std::exit(1); }
  }
  return c;
}

template <typename RNG>
static BenchResult run_bench_fixed(const std::string& name, const Cmd& cmd) {
  BenchResult r; r.name = name; r.threads = cmd.threads;

  const uint64_t per_thread = cmd.total / cmd.threads;
  std::vector<std::thread> ts;
  std::mutex agg_mtx;
  RunningStats agg_stats;
  ByteHist agg_hist;
  std::atomic<uint64_t> total_u64{0}, total_f64{0};
  double time_u64 = 0.0, time_f64 = 0.0;

  auto bench_u64 = [&](unsigned tid){
    splitmix64 seeder(cmd.seed + tid*0x9E3779B97F4A7C15ull);
    RNG rng(seeder.next());

    ScopedTimer t;
    uint64_t local = 0;
    ByteHist h;
    for (uint64_t i=0;i<per_thread;++i) {
      uint64_t x = rng.next_u64();
      h.push_u64(x);
      local++;
    }
    double secs = t.elapsed_sec();

    std::lock_guard<std::mutex> lk(agg_mtx);
    for (int i=0;i<256;++i) agg_hist.bins[i] += h.bins[i];
    total_u64 += local;
    time_u64 += secs;
  };

  auto bench_f64 = [&](unsigned tid){
    splitmix64 seeder(cmd.seed + 0xFACEB00CULL + tid*0x9E37);
    RNG rng(seeder.next());

    ScopedTimer t;
    uint64_t local = 0;
    RunningStats st;
    for (uint64_t i=0;i<per_thread;++i) {
      double d = rng.next_double();
      st.push(d);
      local++;
    }
    double secs = t.elapsed_sec();

    std::lock_guard<std::mutex> lk(agg_mtx);
    // merge: approximate — push the mean as many times is too slow; just weight by n
    // compute combined using formulas:
    // For simplicity here: store per-thread means/vars? To keep code tight, we just accumulate a few samples for reporting.
    // We'll accept slight approximations by combining using parallel formula.
    static long double global_mean = 0.0L;
    static long double global_M2 = 0.0L;
    static uint64_t global_n = 0;

    uint64_t n2 = global_n + st.n;
    if (st.n) {
      long double delta = st.mean - global_mean;
      long double mean_new = global_mean + delta * ( (long double)st.n / (long double)n2 );
      long double M2_new = global_M2 + st.m2 + delta*delta*( (long double)global_n * (long double)st.n / (long double)n2 );
      global_mean = mean_new; global_M2 = M2_new; global_n = n2;

      agg_stats.mean = global_mean;
      agg_stats.m2   = global_M2;
      agg_stats.n    = global_n;
    }

    total_f64 += local;
    time_f64 += secs;
  };

  // u64
  ts.clear();
  for (unsigned t=0;t<cmd.threads;++t) ts.emplace_back(bench_u64, t);
  for (auto& th : ts) th.join();

  // f64
  ts.clear();
  for (unsigned t=0;t<cmd.threads;++t) ts.emplace_back(bench_f64, t);
  for (auto& th : ts) th.join();

  r.total_u64 = total_u64.load();
  r.secs_u64 = time_u64; // sum of thread times; report per-thread avg throughput below
  r.ops_per_s_u64 = (r.total_u64) / (time_u64 / (double)cmd.threads);

  r.total_f64 = total_f64.load();
  r.secs_f64 = time_f64;
  r.ops_per_s_f64 = (r.total_f64) / (time_f64 / (double)cmd.threads);

  r.mean_f64 = (double)agg_stats.mean;
  r.var_f64  = agg_stats.variance();
  r.chi2_bytes = agg_hist.chi_square();
  return r;
}

static BenchResult run_bench_csimd(const std::string& name, const Cmd& cmd, const std::string& libpath, int algo_id, int bitwidth) {
  BenchResult r; r.name = name; r.threads = cmd.threads;

  CSimdLib lib(libpath);

  const uint64_t per_thread = cmd.total / cmd.threads;
  std::vector<std::thread> ts;
  std::mutex agg_mtx;
  RunningStats agg_stats;
  ByteHist agg_hist;
  std::atomic<uint64_t> total_u64{0}, total_f64{0};
  double time_u64 = 0.0, time_f64 = 0.0;

  auto bench_u64 = [&](unsigned tid){
    splitmix64 seeder(cmd.seed + tid*0x9E3779B97F4A7C15ull);
    CSimdLib::Instance rng(&lib, seeder.next(), algo_id, bitwidth);

    ScopedTimer t;
    uint64_t local = 0;
    ByteHist h;
    for (uint64_t i=0;i<per_thread;++i) {
      uint64_t x = rng.next_u64();
      h.push_u64(x);
      local++;
    }
    double secs = t.elapsed_sec();

    std::lock_guard<std::mutex> lk(agg_mtx);
    for (int i=0;i<256;++i) agg_hist.bins[i] += h.bins[i];
    total_u64 += local;
    time_u64 += secs;
  };

  auto bench_f64 = [&](unsigned tid){
    splitmix64 seeder(cmd.seed + 0xFACEB00CULL + tid*0x9E37);
    CSimdLib::Instance rng(&lib, seeder.next(), algo_id, bitwidth);

    ScopedTimer t;
    uint64_t local = 0;
    RunningStats st;
    for (uint64_t i=0;i<per_thread;++i) {
      double d = rng.next_double();
      st.push(d);
      local++;
    }
    double secs = t.elapsed_sec();

    std::lock_guard<std::mutex> lk(agg_mtx);
    static long double global_mean = 0.0L;
    static long double global_M2 = 0.0L;
    static uint64_t global_n = 0;

    uint64_t n2 = global_n + st.n;
    if (st.n) {
      long double delta = st.mean - global_mean;
      long double mean_new = global_mean + delta * ( (long double)st.n / (long double)n2 );
      long double M2_new = global_M2 + st.m2 + delta*delta*( (long double)global_n * (long double)st.n / (long double)n2 );
      global_mean = mean_new; global_M2 = M2_new; global_n = n2;

      agg_stats.mean = global_mean;
      agg_stats.m2   = global_M2;
      agg_stats.n    = global_n;
    }

    total_f64 += local;
    time_f64 += secs;
  };

  // u64
  ts.clear();
  for (unsigned t=0;t<cmd.threads;++t) ts.emplace_back(bench_u64, t);
  for (auto& th : ts) th.join();
  // f64
  ts.clear();
  for (unsigned t=0;t<cmd.threads;++t) ts.emplace_back(bench_f64, t);
  for (auto& th : ts) th.join();

  r.total_u64 = total_u64.load();
  r.secs_u64 = time_u64;
  r.ops_per_s_u64 = (r.total_u64) / (time_u64 / (double)cmd.threads);

  r.total_f64 = total_f64.load();
  r.secs_f64 = time_f64;
  r.ops_per_s_f64 = (r.total_f64) / (time_f64 / (double)cmd.threads);

  r.mean_f64 = (double)agg_stats.mean;
  r.var_f64  = agg_stats.variance();
  r.chi2_bytes = agg_hist.chi_square();
  return r;
}

static void print_table(const std::vector<BenchResult>& R) {
  auto w = [](int n){ return std::setw(n); };
  std::cout << std::left
    << w(20) << "generator"
    << w(16) << "u64 ops/s"
    << w(16) << "f64 ops/s"
    << w(12) << "mean(f64)"
    << w(12) << "var(f64)"
    << w(14) << "chi2(bytes)"
    << w(8)  << "threads"
    << "\n";

  std::cout << std::string(20+16+16+12+12+14+8, '-') << "\n";
  std::cout << std::fixed << std::setprecision(2);
  for (auto& r : R) {
    auto fmt = [](double x)->std::string{
      std::ostringstream ss; ss<<std::fixed<<std::setprecision(2)<<x/1e6<<" M/s"; return ss.str();
    };
    std::cout << std::left
      << w(20) << r.name
      << w(16) << fmt(r.ops_per_s_u64)
      << w(16) << fmt(r.ops_per_s_f64)
      << w(12) << std::setprecision(6) << r.mean_f64
      << w(12) << std::setprecision(6) << r.var_f64
      << w(14) << std::setprecision(2) << r.chi2_bytes
      << w(8)  << r.threads
      << "\n";
  }
}

int main(int argc, char** argv) {
  Cmd cmd = parse(argc, argv);

  // default list:
  if (cmd.gens.empty()) {
    cmd.gens = {
      "std_mt19937",
      "std_mt19937_64",
      "std_minstd",
      "ranlux48",
      "xoroshiro128pp",
//      "xoshiro256ss",
      "pcg32",
      "csimd"
    };
  }

  std::vector<BenchResult> results;

  auto wants = [&](const char* tag){
    return std::find(cmd.gens.begin(), cmd.gens.end(), std::string(tag)) != cmd.gens.end();
  };

  if (wants("std_mt19937")) {
    results.push_back(run_bench_fixed<std_mt19937>("std_mt19937", cmd));
  }
  if (wants("std_mt19937_64")) {
    results.push_back(run_bench_fixed<std_mt19937_64>("std_mt19937_64", cmd));
  }
  if (wants("std_minstd")) {
    results.push_back(run_bench_fixed<std_minstd_rand>("minstd_rand", cmd));
  }
  if (wants("ranlux48")) {
    results.push_back(run_bench_fixed<std_ranlux48>("ranlux48", cmd));
  }
  if (wants("xoroshiro128pp")) {
    results.push_back(run_bench_fixed<xoroshiro128pp>("xoroshiro128pp", cmd));
  }
//  if (wants("xoshiro256ss")) {
//    results.push_back(run_bench_fixed<xoshiro256ss>("xoshiro256ss", cmd));
//  }
  if (wants("pcg32")) {
    results.push_back(run_bench_fixed<pcg32>("pcg32", cmd));
  }
  if (wants("csimd")) {
    if (cmd.csimd_path.empty()) {
      std::fprintf(stderr, "[warn] --csimd-lib not provided; skipping 'csimd'\n");
    } else {
      try {
        auto r = run_bench_csimd("csimd_universal", cmd, cmd.csimd_path, cmd.csimd_algo, cmd.csimd_bitwidth);
        results.push_back(std::move(r));
      } catch (const std::exception& e) {
        std::fprintf(stderr, "[error] csimd: %s\n", e.what());
      }
    }
  }

  print_table(results);

  if (!cmd.csv_path.empty()) {
    CsvWriter w(cmd.csv_path);
    if (w) {
      w.header({"generator","u64_ops_per_s","f64_ops_per_s","mean_f64","var_f64","chi2_bytes","threads","total_u64","total_f64"});
      for (auto& r : results) {
        w.write({
          r.name,
          std::to_string(r.ops_per_s_u64),
          std::to_string(r.ops_per_s_f64),
          std::to_string(r.mean_f64),
          std::to_string(r.var_f64),
          std::to_string(r.chi2_bytes),
          std::to_string(r.threads),
          std::to_string(r.total_u64),
          std::to_string(r.total_f64)
        });
      }
      w.flush();
      std::fprintf(stderr, "[info] wrote CSV: %s\n", cmd.csv_path.c_str());
    } else {
      std::fprintf(stderr, "[warn] failed to open CSV for write: %s\n", cmd.csv_path.c_str());
    }
  }

  return 0;
}
