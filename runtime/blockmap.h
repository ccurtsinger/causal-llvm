#if !defined(CAUSAL_RUNTIME_BLOCKMAP_H)
#define CAUSAL_RUNTIME_BLOCKMAP_H

#include <dlfcn.h>
#include <map>
#include <string>

#include "log.h"

using namespace std;

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

class file {
private:
  const char* _name;
  uintptr_t _base;
  //map<interval, function*> _functions;
  
public:
  file(const char* name, uintptr_t base) : _name(name), _base(base) {}
  
  const char* getName() {
    return _name;
  }
  
  ptrdiff_t getOffset(void* p) {
    return (uintptr_t)p - _base;
  }
  
  void* getFunction(void* p) {
    Dl_info info;
    REQUIRE(dladdr(p, &info) != 0, "Failed to located symbol for %p", p);
    return info.dli_saddr;
  }
};

class blockmap {
private:
  map<interval, file*> _files;
  
public:
  void addFile(const char* name, void* text_start, void* text_end) {
    interval i((uintptr_t)text_start, (uintptr_t)text_end);
    PREFER(_files.find(i) == _files.end(), "Blockmap has overlapping file ranges!");
    _files[i] = new file(name, (uintptr_t)text_start);
  }
  
  file* getFile(void* p) {
    map<interval, file*>::iterator iter = _files.find(p);
    if(iter == _files.end()) return NULL;
    else return iter->second;
  }
  
  void* getFunction(void* p) {
    file* f = getFile(p);
    if(f != NULL) {
      return f->getFunction(p);
    } else {
      return NULL;
    }
  }
  
  bool inBounds(void* p) {
    map<interval, file*>::iterator iter = _files.find(p);
    return iter != _files.end();
  }
};

#endif
