#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

struct CsvRow {
  std::vector<std::string> cols;
};
struct CsvWriter {
  std::ofstream out;
  explicit CsvWriter(const std::string& path) : out(path, std::ios::out | std::ios::trunc) {}
  void header(const std::vector<std::string>& cols) {
    write(cols);
  }
  void write(const std::vector<std::string>& cols) {
    for (size_t i=0;i<cols.size();++i) {
      if (i) out << ',';
      // naive escape
      bool needq = cols[i].find_first_of(",\"\n") != std::string::npos;
      if (needq) out << '"';
      for (char c: cols[i]) {
        if (c=='"') out << "\"\"";
        else out << c;
      }
      if (needq) out << '"';
    }
    out << "\n";
  }
  void flush() { out.flush(); }
  explicit operator bool() const { return out.good(); }
};
