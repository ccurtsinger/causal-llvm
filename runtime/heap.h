#if !defined(CAUSAL_RUNTIME_HEAP_H)
#define CAUSAL_RUNTIME_HEAP_H

#include <heaplayers>
#include <string>

using HL::BumpAlloc;
using HL::LockedHeap;
using HL::MmapHeap;
using HL::PosixLockType;

typedef LockedHeap<PosixLockType, BumpAlloc<0x200000, PrivateMmapHeap>> CausalHeap;

typedef basic_string<char, char_traits<char>, STLAllocator<char, CausalHeap>> causal_string;

#endif
