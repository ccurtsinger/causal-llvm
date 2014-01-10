#if !defined(CAUSAL_RUNTIME_FUNCTION_H)
#define CAUSAL_RUNTIME_FUNCTION_H

#include <atomic>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stack>

#include <dlfcn.h>
#include <stdint.h>
#include <sys/mman.h>

#include "disassembler.h"
#include "heap.h"
#include "interval.h"
#include "log.h"

using std::atomic_flag;
using std::deque;
using std::less;
using std::map;
using std::pair;
using std::set;
using std::stack;

class Function {
public:
  class BasicBlock {
  private:
    Function& _f;
    size_t _index;
    interval _range;
    size_t _length;
    atomic<size_t> _cycle_samples = ATOMIC_VAR_INIT(0);
    atomic<size_t> _inst_samples = ATOMIC_VAR_INIT(0);
    
    BasicBlock() : BasicBlock(&Function::getNullFunction(), 0, interval(0, 0)) {}
    
  public:
    BasicBlock(Function* f, size_t index, interval range) : _f(*f), _index(index), _range(range) {}
    BasicBlock(const BasicBlock& other) : _f(other._f), _index(other._index), 
        _range(other._range), _length(other._length) {
      _cycle_samples.store(other._cycle_samples);
      _inst_samples.store(other._inst_samples);
    }
    
    ~BasicBlock() {
      if(_cycle_samples > 0 || _inst_samples > 0) {
        fprintf(stderr, "%s block %lu\n  %lu cycle samples\n  %lu instruction samples\n", 
          _f.getName().c_str(), _index,
          _cycle_samples.load(),
          _inst_samples.load());
      }
    }
    
    static BasicBlock& getNullBlock() {
      static BasicBlock _null_block;
      return _null_block;
    }
    
    size_t getIndex() {
      return _index;
    }
    
    void cycleSample() {
      _cycle_samples++;
    }
    
    void instructionSample() {
      _inst_samples++;
    }
  };
  
private:
  typedef map<interval, BasicBlock, less<interval>, 
    STLAllocator<pair<const interval, BasicBlock>, CausalHeap>> block_map;
  typedef set<uintptr_t, less<uintptr_t>,
    STLAllocator<uintptr_t, CausalHeap>> uintptr_set;
  typedef stack<uintptr_t,
    deque<uintptr_t, STLAllocator<uintptr_t, CausalHeap>>> uintptr_stack;
  
  causal_string _name;
  interval _range;
  atomic_flag _lock = ATOMIC_FLAG_INIT;
  bool _processed = false;
  block_map _blocks;
  
  void findBlocks() {
    if(_processed) return;
    
    while(_lock.test_and_set()) {
      __asm__("pause");
    }
    
    if(!_processed) {
      // Disassemble to find starting addresses of all basic blocks
      uintptr_set block_bases;
      uintptr_stack q;
      q.push(_range.getBase());
    
      while(q.size() > 0) {
        uintptr_t p = q.top();
        q.pop();
      
        // Skip null or already-seen pointers
        if(p == 0 || block_bases.find(p) != block_bases.end())
          continue;
      
        // This is a new block starting address
        block_bases.insert(p);
      
        // Disassemble the new basic block
        for(disassembler inst = disassembler(p, _range.getLimit()); !inst.done() && inst.fallsThrough(); inst.next()) {
          if(inst.branches()) {
            branch_target target = inst.target();
            if(target.dynamic()) {
              WARNING("Unhandled dynamic branch target in instruction %s", inst.toString());
            } else {
              uintptr_t t = target.value();
              if(_range.contains(t))
                q.push(t);
            }
          }
        }
      }
    
      // Create basic block objects
      size_t index = 0;
      uintptr_t prev_base = 0;
      for(uintptr_set::iterator iter = block_bases.begin(); iter != block_bases.end(); iter++) {
        if(prev_base != 0) {
          interval i(prev_base, *iter);
          _blocks.insert(pair<const interval, BasicBlock>(i, BasicBlock(this, index, i)));
          index++;
        }
        prev_base = *iter;
      }
    
      // The last block ends at the function's limit address
      interval i(prev_base, _range.getLimit());
      _blocks.insert(pair<const interval, BasicBlock>(i, BasicBlock(this, index, i)));
      
      _processed = true;
    }
    
    _lock.clear();
  }
  
  Function() : Function("<null>", interval(0, 0)) {}
  
public:
  Function(causal_string name, interval range) : _name(name), _range(range) {}
  Function(const Function& other) : _name(other._name), _range(other._range), _processed(false) {}

  BasicBlock& getBlock(uintptr_t p) {
    // Ensure this function has been disassembled to identify basic blocks
    findBlocks();
    
    block_map::iterator iter = _blocks.find(p);
    if(iter == _blocks.end()) {
      return BasicBlock::getNullBlock();
    } else {
      return iter->second;
    }
  }
  
  static Function& getNullFunction() {
    static Function _null_Function;
    return _null_Function;
  }

  const causal_string& getName() {
    return _name;
  }
  
  const interval& getRange() {
    return _range;
  }
};

#endif
