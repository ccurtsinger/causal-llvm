#if !defined(CAUSAL_RUNTIME_HEAP_H)
#define CAUSAL_RUNTIME_HEAP_H

#include <heaplayers>
#include <new>
#include <string>

using HL::BumpAlloc;
using HL::LockedHeap;
using HL::MmapHeap;
using HL::PosixLockType;

typedef SizeHeap<LockedHeap<PosixLockType, FreelistHeap<BumpAlloc<0x200000, PrivateMmapHeap>>>> SourceHeap;
typedef KingsleyHeap<SourceHeap, MmapHeap> CausalHeap;

CausalHeap& getPrivateHeap();

class PrivateAllocated {
public:
  /// Override new
  void* operator new(size_t sz) { return getPrivateHeap().malloc(sz); }
  /// Override nothrow version of new
  void* operator new(size_t sz, const std::nothrow_t&) { return getPrivateHeap().malloc(sz); }
  
  /// Override new[]
  void* operator new[](size_t sz) { return getPrivateHeap().malloc(sz); }
  /// Override nothrow version of new[]
  void* operator new[](size_t sz, const std::nothrow_t&) { return getPrivateHeap().malloc(sz); }
  
  /// Override delete
  void operator delete(void* p) { getPrivateHeap().free(p); }
  /// Override nothrow version of delete
  void operator delete(void* p, const std::nothrow_t&) { getPrivateHeap().free(p); }
  
  /// Override delete[]
  void operator delete[](void* p) { getPrivateHeap().free(p); }
  /// Override nothrow version of delete[]
  void operator delete[](void* p, const std::nothrow_t&) { getPrivateHeap().free(p); }
};

#endif
