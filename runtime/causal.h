#if !defined(CAUSAL_RUNTIME_CAUSAL_H)
#define CAUSAL_RUNTIME_CAUSAL_H

#include <cstdint>
#include <cstdio>
#include <map>
#include <new>
#include <set>
#include <stack>
#include <thread>

//#include "basicblock.h"
#include "disassembler.h"
#include "elf.h"
//#include "function.h"
#include "log.h"
#include "papi.h"
#include "real.h"
#include "sampler.h"
#include "util.h"

enum {
  CycleSamplePeriod = 10000000,
  InstructionSamplePeriod = 500000
};

/// Common sample counting class shared by blocks, functions, and files.
/// This allows files and functions to count "orphaned" samples that can't
/// be mapped to a known function or basic block.
class SampleBin {
private:
  size_t _cycle_samples = 0;
  size_t _inst_samples = 0;
public:
  SampleBin() {}
  /// Record a sample of a given type
  void addSample(SampleType t) {
    if(t == SampleType::Cycle)
      _cycle_samples++;
    else
      _inst_samples++;
  }
  // Accessors for sample counters
  size_t getCycleSamples() const { return _cycle_samples; }
  size_t getInstructionSamples() const { return _inst_samples; }
};

class BasicBlock : public SampleBin {
private:
  interval _range;
  bool _entry;
  size_t _length = 0;
public:
  BasicBlock(interval range, bool entry) : _range(range), _entry(entry) {
    // Count instructions
    for(disassembler i(range); !i.done(); i.next()) {
      _length++;
    }
  }
  
  const interval& getRange() const { return _range; }
  bool isEntryBlock() const { return _entry; }
  size_t getLength() const { return _length; }
  
  /// Split this block at the given pointer and return the new successor block
  BasicBlock split(uintptr_t p) {
    BasicBlock b(interval(p, _range.getLimit()), false); // Split off a new block starting at p
    _length -= b.getLength(); // Decrease the instruction count by the length of the new block
    _range = interval(_range.getBase(), p); // Update this block's range
    return b;
  }
};

class Function : public SampleBin {
private:
  std::string _name;
  interval _range;
  uintptr_t _load_offset;
  bool _processed;
public:
  Function(const std::string name, interval range, uintptr_t load_offset) :
    _name(name), _range(range), _load_offset(load_offset), _processed(false) {}
    
  const std::string& getName() const { return _name; }
  interval getRange() const { return _range; }
  interval getLoadedRange() const { return _range + _load_offset; }
  
  // Mark the function as processed when its basic blocks have been identified
  bool isProcessed() const { return _processed; }
  void setProcessed() { _processed = true; }
};

class File : public SampleBin {
private:
  std::string _name;
  interval _range;
public:
  File(const std::string name, interval range) :
    _name(name), _range(range) {}
    
  const std::string& getName() const { return _name; };
  interval getRange() const { return _range; }
};

class Causal {
private:
  bool _initialized;
  pthread_t _profiler_thread;
  
  SampleBin _orphan;
  map<interval, File> _files;
  map<interval, Function> _functions;
  map<interval, BasicBlock> _blocks;
  
	Causal() : _initialized(false) {
    initialize();
	}
  
  SampleBin& getBin(uintptr_t p) {
    // Try to find a matching block. If one exists, return immediately
    map<interval, BasicBlock>::iterator b = _blocks.find(p);
    if(b != _blocks.end()) return b->second;
    
    // No luck. Try to find a matching function
    map<interval, Function>::iterator fn = _functions.find(p);
    if(fn != _functions.end()) {
      if(fn->second.isProcessed()) {
        // If the function has already been processed, then we're not going to find a block
        // Just return the function.
        return fn->second;
      } else {
        // Function hasn't been disassembled yet. Process it
        findBlocks(fn->second.getLoadedRange());
        fn->second.setProcessed();
        // Can we find a block now?
        b = _blocks.find(p);
        if(b != _blocks.end()) return b->second;
        else return fn->second;
      }
    }
    
    // No luck finding a function either. Check for a known file
    map<interval, File>::iterator f = _files.find(p);
    // If found, return the file. Otherwise return the default orphan bin
    if(f != _files.end()) return f->second;
    else return _orphan;
  }
  
  void profiler() {
    while(true) {
      SampleBlock* block = sampler::getNextBlock();
      
      for(Sample& s : block->getSamples()) {
        getBin(s.address).addSample(s.type);
      }
      
      delete block;
    }
  }
  
  static void* startProfiler(void* arg) {
    getInstance().profiler();
    return NULL;
  }
  
  void findBlocks(interval range) {
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
    for(set<uintptr_t>::iterator iter = block_bases.begin(); iter != block_bases.end(); iter++) {
      if(prev_base != 0) {
        interval r(prev_base, *iter);
        _blocks.emplace(r, BasicBlock(r, index == 0));
        index++;
      }
      prev_base = *iter;
    }

    // The last block ends at the function's limit address
    interval r(prev_base, range.getLimit());
    _blocks.emplace(r, BasicBlock(r, index == 0));
  }
  
  void findFunctions() {
    for(const auto& file : papi::getFiles()) {
      const string& filename = file.first;
      const interval& file_range = file.second;
      
      // Record the file range
      _files.emplace(file_range, File(filename, file_range));
      
      // Skip libpapi and libcausal
      if(filename.find("libcausal") != string::npos ||
         filename.find("libpapi") != string::npos) {
        continue;
      }
      
      ELFFile* elf = ELFFile::open(filename);
      
      if(elf == NULL) {
        WARNING("Skipping file %s", filename.c_str());
      } else {
        // Dynamic libraries need to be shifted to their load address
        uintptr_t load_offset = 0;
        if(elf->isDynamic())
          load_offset = file_range.getBase();
        
        for(const auto& fn : elf->getFunctions()) {
          const string& fn_name = fn.first;
          interval fn_range = fn.second;
          
          _functions.emplace(fn_range + load_offset, Function(fn_name, fn_range, load_offset));
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
    if(__atomic_exchange_n(&_initialized, true, __ATOMIC_SEQ_CST) == false) {
      INFO("Initializing");
      
      // Set up PAPI
      papi::initialize();
      
      // Build a map of functions
      findFunctions();
    
      // Create the profiler thread
      REQUIRE(Real::pthread_create()(&_profiler_thread, NULL, startProfiler, NULL) == 0,
        "Failed to create profiler thread");
        
      // Initialize the main thread
      initializeThread();
    }
  }
  
  void reinitialize() {
    INFO("Reinitializing");
    __atomic_store_n(&_initialized, false, __ATOMIC_SEQ_CST);
  }
  
  void shutdown() {
    if(__atomic_exchange_n(&_initialized, false, __ATOMIC_SEQ_CST) == true) {
      INFO("Shutting down");
      
      const File* current_file = NULL;
      const Function* current_fn = NULL;
      
      for(const auto& i : _blocks) {
        const BasicBlock& b  = i.second;
        uintptr_t block_base = b.getRange().getBase();
        // If this block has no samples, skip it
        if(b.getCycleSamples() == 0 && b.getInstructionSamples() == 0)
          continue;
        
        // If this is a new file, print info
        if(current_file == NULL || !current_file->getRange().contains(block_base)) {
          current_file = &_files.find(block_base)->second;
          fprintf(stderr, "%s (%p-%p)\n", current_file->getName().c_str(),
            (void*)current_file->getRange().getBase(),
            (void*)current_file->getRange().getLimit());
        }
        
        // If this is a new function, print info
        if(current_fn == NULL || !current_fn->getLoadedRange().contains(block_base)) {
          current_fn = &_functions.find(block_base)->second;
          fprintf(stderr, "  %s (%p-%p)\n", current_fn->getName().c_str(),
            (void*)current_fn->getLoadedRange().getBase(),
            (void*)current_fn->getLoadedRange().getLimit());
        }
        
        fprintf(stderr, "    block at +%lu (%lu instructions)\n",
          block_base - current_fn->getLoadedRange().getBase(),
          b.getLength());
        fprintf(stderr, "      cycle samples: %lu\n", b.getCycleSamples());
        fprintf(stderr, "      instruction samples: %lu\n", b.getInstructionSamples());
      }
    }
  }
  
  void initializeThread() {
    sampler::initializeThread(CycleSamplePeriod, InstructionSamplePeriod);
  }
  
  void shutdownThread() {
    sampler::shutdownThread();
  }
};

#endif
