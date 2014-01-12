#include "engine.h"

#include <dlfcn.h>
#include <papi.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

#include "elf.h"
#include "function.h"
#include "interval.h"
#include "log.h"
#include "papi.h"
#include "samples.h"
#include "util.h"

using std::pair;
using std::set;

namespace engine {
  enum {
    CycleSamplePeriod = 10000000,
    CycleSampleMask = 0x1,
    InstructionSamplePeriod = 5000000,
    InstructionSampleMask = 0x2
  };
  
  map<interval, Function> _buildFunctionMap() {
    map<interval, Function> functions;
    
    for(const auto& file : papi::getFiles()) {
      const string& filename = file.first;
      const interval& file_range = file.second;
      
      // Skip libpapi and libcausal
      if(filename.find("libcausal") != string::npos ||
         filename.find("libpapi") != string::npos) {
        continue;
      }
      
      ELFFile* elf = ELFFile::open(filename);
      
      if(elf == NULL) {
        WARNING("Skipping file %s", filename.c_str());
      } else {
        for(const auto& fn : elf->getFunctions()) {
          const string& fn_name = fn.first;
          interval fn_range = fn.second;
          
          // Dynamic libraries need to be shifted to their load address
          if(elf->isDynamic())
            fn_range += file_range.getBase();
          
          functions.emplace(fn_range, Function(fn_name, fn_range));
        }
        
        delete elf;
      }
    }
    
    return functions;
  }
  
  map<interval, Function>& getFunctionMap() {
    static map<interval, Function> functions = _buildFunctionMap();
    return functions;
  }
  
  Function& getFunction(uintptr_t p) {
    map<interval, Function>& functions = getFunctionMap();
    map<interval, Function>::iterator iter = functions.find(p);
    if(iter != functions.end()) return iter->second;
    else return Function::getNullFunction();
  }
  
  void overflowHandler(int event_set, void* address, long long vec, void* context) {
    if(vec & CycleSampleMask) {
      SampleChunk::getLocal()->add(SampleType::Cycle, (uintptr_t)address);
    }

    if(vec & InstructionSampleMask) {
      SampleChunk::getLocal()->add(SampleType::Instruction, (uintptr_t)address);
    }
  }

  void initialize() {
    papi::initialize();
    getFunctionMap();
  }
  
  void addThread() {
    papi::startThread(CycleSamplePeriod, InstructionSamplePeriod, overflowHandler);
  }
  
  void removeThread() {
    papi::stopThread();
    SampleChunk::flushLocal();
  }
}
