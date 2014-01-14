#if !defined(CAUSAL_RUNTIME_OUTPUT_H)
#define CAUSAL_RUNTIME_OUTPUT_H

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fstream>

#include "bins.h"
#include "interval.h"
#include "log.h"

class Output {
private:
  std::ofstream f;
public:
  Output(const char* basename, size_t cycle_period, size_t inst_period) {
    f.open("out.czl", std::ofstream::out | std::ofstream::app);
    REQUIRE(f.is_open(), "Failed to open out.czl for output");
    
    f << "basename\t" << basename << "\n";
    f << "cycle period\t" << cycle_period << "\n";
    f << "instruction period\t" << inst_period << "\n";
  }
  
  ~Output() {
    f.close();
  }
  
  void writeBlockStats(const std::string& filename, const std::string& function_name, const BasicBlock& block) {
    f << "blockstats\t" << filename << "\t" << function_name << "\t" << block << "\n";
  }
};

#endif
