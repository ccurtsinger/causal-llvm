#if !defined(CAUSAL_RUNTIME_BASICBLOCK_H)
#define CAUSAL_RUNTIME_BASICBLOCK_H

#include "function.h"
#include "interval.h"

class Function;

class BasicBlock {
private:
  Function& _f;
  size_t _index;
  interval _range;
  size_t _length;
  size_t _cycle_samples = 0;
  size_t _inst_samples = 0;
  
  /// Default constructor for the null basic block object
  BasicBlock();
  
public:
  /// Track a basic block in a given function
  BasicBlock(Function* f, size_t index, interval range);
  
  /// Output profiling data when destroyed
  ~BasicBlock();
  
  /// Get a basic block object for unidentified samples
  static BasicBlock& getNullBlock() {
    static BasicBlock _null_block;
    return _null_block;
  }
  
  /// Get this block's index
  inline size_t getIndex() { return _index; }
  
  /// Record a sample from the cycle counter
  inline void cycleSample() { _cycle_samples++; }
  
  /// Record a sample from the instruction counter
  inline void instructionSample() { _inst_samples++; }
};

#endif
