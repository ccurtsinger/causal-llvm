#if !defined(CAUSAL_RUNTIME_CAUSAL_H)
#define CAUSAL_RUNTIME_CAUSAL_H

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <set>
#include <stack>
#include <sstream>
#include <vector>

#include "counter.h"
#include "disassembler.h"
#include "elf.h"
#include "log.h"
#include "papi.h"
#include "real.h"
#include "sampler.h"
#include "util.h"

enum {
  CycleSamplePeriod = 10000000,
  InstructionSamplePeriod = 500011
};

class Causal {
private:
  std::atomic<bool> _initialized = ATOMIC_VAR_INIT(false);
  size_t _start_time;
  bool _speedup;
  size_t _delay;
  std::vector<Counter*> _progress_counters;
  
	Causal() {
    initialize();
	}
  
  void printCounters() {
    size_t i = 0;
    for(Counter* c : _progress_counters) {
      fprintf(stderr, "%lu:%lu\n", i, c->getValue());
      i++;
    }
  }
  
  void dumpBlocks(interval range) {
    // Disassemble to find starting addresses of all basic blocks
    std::set<uintptr_t> block_bases;
    std::stack<uintptr_t> q;
    q.push(range.getBase());

    while(q.size() > 0) {
      uintptr_t p = q.top();
      q.pop();
  
      // Skip null or already-seen pointers
      if(p == 0 || block_bases.find(p) != block_bases.end())
        continue;
  
      // This is a new block starting address
      block_bases.insert(p);
  
      // Disassemble the new basic block
      for(disassembler inst = disassembler(p, range.getLimit()); !inst.done() && inst.fallsThrough(); inst.next()) {
        if(inst.branches()) {
          branch_target target = inst.target();
          if(target.dynamic()) {
            WARNING("Unhandled dynamic branch target in instruction %s", inst.toString());
          } else {
            uintptr_t t = target.value();
            if(range.contains(t))
              q.push(t);
          }
        }
      }
    }

    // Create basic block objects
    size_t index = 0;
    uintptr_t prev_base = 0;
    for(std::set<uintptr_t>::iterator iter = block_bases.begin(); iter != block_bases.end(); iter++) {
      if(prev_base != 0) {
        size_t length = 1;
        for(disassembler i(prev_base, *iter); !i.done() && i.fallsThrough(); i.next()) {
          length++;
        }
        fprintf(stderr, "  %lu: %p-%p, %lu instructions\n", index, (void*)prev_base, (void*)*iter, length);
        index++;
      }
      prev_base = *iter;
    }

    // The last block ends at the function's limit address
    size_t length = 0;
    for(disassembler i(prev_base, range.getLimit()); !i.done() && i.fallsThrough(); i.next()) {
      length++;
    }
    fprintf(stderr, "  %lu: %p-%p %lu instructions\n", index, (void*)prev_base, (void*)range.getLimit(), length);
  }
  
  void dumpBlocks() {
    for(const auto& file : papi::getFiles()) {
      const string& filename = file.first;
      const interval& file_range = file.second;
    
      ELFFile* elf = ELFFile::open(filename);
    
      if(elf == NULL) {
        WARNING("Skipping file %s", filename.c_str());
      } else {
        // Dynamic libraries need to be shifted to their load address
        uintptr_t load_offset = 0;
        if(elf->isDynamic()) {
          continue;
          load_offset = file_range.getBase();
        }
      
        for(const auto& fn : elf->getFunctions()) {
          const string& fn_name = fn.first;
          interval fn_range = fn.second;
        
          fprintf(stderr, "%s\n", fn_name.c_str());
          dumpBlocks(fn_range + load_offset);
        }
      
        delete elf;
      }
    }
  }
  
public:
	static Causal& getInstance() {
		static char buf[sizeof(Causal)];
		static Causal* instance = new(buf) Causal();
		return *instance;
	}
  
  void initialize() {
    if(_initialized.exchange(true) == false) {
      INFO("Initializing");
      
      // Set up PAPI
      papi::initialize();
      
      char* mode_c = getenv("CAUSAL_MODE");
      REQUIRE(mode_c != NULL, "CAUSAL_MODE is not set");
      
      if(string(mode_c) == "dump") {
        dumpBlocks();
        Real::exit()(0);
      }
      
      std::istringstream s(mode_c);
      
      char speedup_c;
      s.get(speedup_c);
      
      if(speedup_c == '+') {
        _speedup = true;
      } else if(speedup_c == '-') {
        _speedup = false;
      } else {
        FATAL("Mode must specify speedup (+) or slowdown (-)");
      }
      
      // skip up one char
      REQUIRE(s.get() == ' ', "Expected ' '");
      
      uintptr_t base, limit;
      REQUIRE(s.get() == '0', "Expected '0'");
      REQUIRE(s.get() == 'x', "Expected 'x'");
      s >> std::hex >> base;
      REQUIRE(s.get() == '-', "Expected '-'");
      
      REQUIRE(s.get() == '0', "Expected '0'");
      REQUIRE(s.get() == 'x', "Expected 'x'");
      s >> limit;
      
      REQUIRE(s.get() == ' ', "Expected ' '");
      
      s >> std::dec >> _delay;
      
      INFO("Running in %s mode for %p-%p with a delay of %lums",
        _speedup ? "speedup" : "slowdown",
        (void*)base,
        (void*)limit,
        _delay);
      
      if(_speedup)
        sampler::speedup(interval(base, limit), _delay);
      else
        sampler::slowdown(interval(base, limit), _delay);
      
      _start_time = getTime();
        
      // Initialize the main thread
      initializeThread();
    }
  }
  
  void addProgressCounter(Counter* c) {
    _progress_counters.push_back(c);
  }
  
  void addBeginCounter(Counter* c) {
    WARNING("Transaction counters are not supported yet");
  }
  
  void addEndCounter(Counter* c) {
    WARNING("Transaction counters are not supported yet");
  }
  
  void initializeThread() {
    sampler::initializeThread(CycleSamplePeriod, InstructionSamplePeriod);
  }
  
  void shutdownThread() {
    sampler::shutdownThread();
  }
  
  void reinitialize() {
    INFO("Reinitializing");
    _initialized.store(false);
    initialize();
  }
  
  void shutdown() {
    if(_initialized.exchange(false) == true) {
      INFO("Shutting down");
      
      size_t elapsed = getTime() - _start_time;
      
      size_t delay_count = sampler::getExecutedDelays();
      if(_speedup) {
        elapsed -= delay_count * _delay;
      }
      
      float elapsed_s = (float)elapsed / Time_s;
      
      size_t index = 0;
      for(const auto& ctr : _progress_counters) {
        fprintf(stderr, "%lu: %fHz\n", index, ctr->getValue() / elapsed_s);
      }
      
      //fprintf(stderr, "%fms elapsed\n", (float)elapsed / Time_ms);
    }
  }
};

#endif
