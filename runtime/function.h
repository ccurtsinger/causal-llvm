#if !defined(CAUSAL_RUNTIME_FUNCTION_H)
#define CAUSAL_RUNTIME_FUNCTION_H

#include <limits>
#include <set>
#include <stack>

#include <dlfcn.h>
#include <stdint.h>

#include "disassembler.h"
#include "log.h"

using std::set;
using std::stack;

class function {
private:
  string _name;
  uintptr_t _base;
  uintptr_t _limit;
  set<uintptr_t> _blocks;
  
  function(string name, uintptr_t base) : _name(name), _base(base), _limit(base) {
    stack<uintptr_t> q;
    q.push(base);
    
    while(q.size() > 0) {
      uintptr_t p = q.top();
      q.pop();
      
      // Skip null or already-seen pointers
      if(p == 0 || _blocks.find(p) != _blocks.end())
        continue;
      
      // This is a new block starting address
      _blocks.insert(p);
      
      // Disassemble the new basic block
      for(disassembler inst = disassembler(p); !inst.done(); inst.next()) {
        if(inst.limit() > _limit)
          _limit = inst.limit();
        
        if(inst.branches()) {
          branch_target target = inst.target();
          if(target.dynamic()) {
            WARNING("Unhandled dynamic branch target in instruction %s", (const char*)inst);
          } else {
            q.push((uintptr_t)target);
          }
        }
      }
    }
    
    INFO("Function %s: %p-%p", _name.c_str(), (void*)_base, (void*)_limit);
    size_t i=0;
    for(set<uintptr_t>::iterator iter = _blocks.begin(); iter != _blocks.end(); iter++) {
      INFO("  Block %lu: %p", i, (void*)*iter);
      i++;
    }
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
};

#endif
