#if !defined(CAUSAL_RUNTIME_FUNCTION_H)
#define CAUSAL_RUNTIME_FUNCTION_H

#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stack>

#include <dlfcn.h>
#include <stdint.h>

#include "disassembler.h"
#include "interval.h"
#include "log.h"

using std::map;
using std::ostringstream;
using std::set;
using std::stack;

class function;

class basic_block {
private:
  interval _range;
  function* _f;
  size_t _index;
  
public:
  basic_block(function* f, size_t index, uintptr_t base, uintptr_t limit) : 
    _f(f), _index(index), _range(base, limit) {}
  
  static std::pair<const interval, basic_block>
  makeEntry(function* f, size_t index, uintptr_t base, uintptr_t limit) {
    return std::pair<const interval, basic_block>(interval(base, limit), basic_block(f, index, base, limit));
  }
  
  function& getFunction() {
    return *_f;
  }
  
  size_t getIndex() {
    return _index;
  }
  
  string toString();
};

class function {
private:
  string _name;
  uintptr_t _base;
  uintptr_t _limit;
  map<interval, basic_block> _blocks;
  
  function(string name, uintptr_t base) : _name(name), _base(base), _limit(base) {
    // Disassemble to find starting addresses of all basic blocks
    set<uintptr_t> block_bases;
    stack<uintptr_t> q;
    q.push(base);
    
    while(q.size() > 0) {
      uintptr_t p = q.top();
      q.pop();
      
      // Skip null or already-seen pointers
      if(p == 0 || block_bases.find(p) != block_bases.end())
        continue;
      
      // This is a new block starting address
      block_bases.insert(p);
      
      // Disassemble the new basic block
      for(disassembler inst = disassembler(p); !inst.done(); inst.next()) {
        if(inst.limit() > _limit)
          _limit = inst.limit();
        
        if(inst.branches()) {
          branch_target target = inst.target();
          if(target.dynamic()) {
            WARNING("Unhandled dynamic branch target in instruction %s", inst.toString());
          } else {
            q.push(target.value());
          }
        }
      }
    }
    
    // Create basic block objects
    size_t index = 0;
    uintptr_t prev_base = 0;
    for(set<uintptr_t>::iterator iter = block_bases.begin(); iter != block_bases.end(); iter++) {
      if(prev_base != 0) {
        _blocks.insert(basic_block::makeEntry(this, index, prev_base, *iter));
        index++;
      }
      prev_base = *iter;
    }
    
    // The last block ends at the function's limit address
    _blocks.insert(basic_block::makeEntry(this, index, prev_base, _limit));
  }
  
public:
  static function* get(void* p) {
    Dl_info info;
    
    if(dladdr(p, &info) == 0) {
      // No symbol found. Just give up
      return NULL;
    } else if(info.dli_saddr != NULL) {
      // Return a new function object
      return new function(info.dli_sname, (uintptr_t)info.dli_saddr);
    } else {
      return NULL;
    }
  }
  
  basic_block* getBlock(void* p) {
    // Use interval's point constructor to search for blocks containing p
    map<interval, basic_block>::iterator iter = _blocks.find(p);
    if(iter == _blocks.end())
      return NULL;
    else
      return &iter->second;
  }
  
  const string& getName() {
    return _name;
  }
};

string basic_block::toString() {
  ostringstream ss;
  ss << _f->getName() << " block " << _index;
  return ss.str();
}

#endif
