#include "engine.h"

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <papi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <set>
#include <string>

#include "disassembler.h"
#include "elf.h"
#include "function.h"
#include "interval.h"
#include "log.h"
#include "papi.h"
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
    
    Dl_info dlinfo;
  	const PAPI_exe_info_t* info = PAPI_get_executable_info();
  	if(info) {
      // Find the load address for the main executable file
      dlinfo.dli_fbase = NULL;
      dladdr(info->address_info.text_start, &dlinfo);
      REQUIRE(dlinfo.dli_fbase != NULL, "Couldn't find base for file %s", info->fullname);
      
      // Process the ELF file for the main executable
      for(pair<const interval, string>& fn : getFunctions(info->fullname, dlinfo.dli_fbase)) {
        functions.emplace(fn.first, Function(fn.second.c_str(), fn.first));
      }
  	}
    
    const PAPI_shlib_info_t* lib_info = PAPI_get_shared_lib_info();
    if(lib_info) {
      for(int i=0; i<lib_info->count; i++) {
        // Don't include PAPI or causal
        if(string(lib_info->map[i].name).find("libpapi") == string::npos &&
           string(lib_info->map[i].name).find("libcausal") == string::npos) {
             
          // Find the load address for this shared library
          dlinfo.dli_fbase = NULL;
          dladdr(lib_info->map[i].text_start, &dlinfo);
          REQUIRE(dlinfo.dli_fbase != NULL, "Couldn't file base for file %s", lib_info->map[i].name);
          
          // Process the ELF file for this library
          for(pair<const interval, string>& fn : getFunctions(lib_info->map[i].name, dlinfo.dli_fbase)) {
            functions.emplace(fn.first, Function(fn.second.c_str(), fn.first));
          }
        }
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
    Function& f = getFunction((uintptr_t)address);
    Function::BasicBlock& b = f.getBlock((uintptr_t)address);
    
    if(vec & CycleSampleMask) {
      b.cycleSample();
    }

    if(vec & InstructionSampleMask) {
      b.instructionSample();
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
  }
}
