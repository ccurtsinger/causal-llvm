#if !defined(CAUSAL_RUNTIME_INTERVAL_H)
#define CAUSAL_RUNTIME_INTERVAL_H

#include "stdint.h"

class interval {
private:
  uintptr_t _base;
  uintptr_t _limit;
  
public:
  interval(uintptr_t base, uintptr_t limit) : _base(base), _limit(limit) {}
  interval(void* base, void* limit) : _base((uintptr_t)base), _limit((uintptr_t)limit) {}
  
  // Unit intervals:
  interval(uintptr_t p) : _base(p), _limit(p+1) {}
  interval(void* p) : _base((uintptr_t)p), _limit((uintptr_t)p + 1) {}
  
  bool operator<(const interval& b) const {
    return _limit <= b._base;
  }
};

#endif
