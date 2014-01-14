#if !defined(CAUSAL_RUNTIME_BINS_H)
#define CAUSAL_RUNTIME_BINS_H

#include <cstdint>
#include <iostream>
#include <string>

#include "disassembler.h"
#include "interval.h"
#include "sampler.h"

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
  size_t _length = 1;
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
  
  void print(ostream& os) const {
    os << _range << "\t" << getLength() << "\t" << getCycleSamples() << "\t" << getInstructionSamples();
  }
};

static ostream& operator<<(ostream& os, const BasicBlock& b) {
  b.print(os);
  return os;
}

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

#endif
