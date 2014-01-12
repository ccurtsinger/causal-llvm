#if !defined(CAUSAL_RUNTIME_FUNCTION_H)
#define CAUSAL_RUNTIME_FUNCTION_H

#include <map>
#include <set>
#include <stack>

#include "basicblock.h"
#include "disassembler.h"
#include "interval.h"
#include "log.h"

using std::deque;
using std::map;
using std::pair;
using std::set;
using std::stack;

class BasicBlock;

class Function {
private:
  string _name;
  interval _range;
  bool _processed = false;
  map<interval, BasicBlock> _blocks;
  
  /// Populate the basic block map for this function
  void findBlocks();
  
  /// Construct a NULL function object
  Function() : Function("<null>", interval()) {}
  
public:
  /// Track a function at a given address range
  Function(string name, interval range) : _name(name), _range(range) {}
  Function(const Function& other) : _name(other._name), _range(other._range), _processed(false) {}

  /// Get the basic block that contains a given pointer
  BasicBlock& getBlock(uintptr_t p);
  
  /// Return a single object as a "null" function
  static Function& getNullFunction() {
    static Function _null_Function;
    return _null_Function;
  }

  inline const string& getName() {
    return _name;
  }
  
  inline const interval& getRange() {
    return _range;
  }
};

#endif
