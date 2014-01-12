#include "function.h"

#include "basicblock.h"

BasicBlock& Function::getBlock(uintptr_t p) {
  // Ensure this function has been disassembled to identify basic blocks
  findBlocks();
  
  map<interval, BasicBlock>::iterator iter = _blocks.find(p);
  if(iter == _blocks.end()) {
    return BasicBlock::getNullBlock();
  } else {
    return iter->second;
  }
}

void Function::findBlocks() {
  if(_processed) return;
  
  // Disassemble to find starting addresses of all basic blocks
  set<uintptr_t> block_bases;
  stack<uintptr_t> q;
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
  for(set<uintptr_t>::iterator iter = block_bases.begin(); iter != block_bases.end(); iter++) {
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
