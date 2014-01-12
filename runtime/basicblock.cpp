#include "basicblock.h"

#include <cstdio>
#include <string>

#include "function.h"
#include "interval.h"

BasicBlock::BasicBlock() : 
  BasicBlock(&Function::getNullFunction(), 0, interval()) {}

BasicBlock::BasicBlock(Function* f, size_t index, interval range) : 
  _f(*f), _index(index), _range(range) {}

BasicBlock::~BasicBlock() {
  if(_cycle_samples > 0 || _inst_samples > 0) {
    fprintf(stderr, "%s block %lu\n  %lu cycle samples\n  %lu instruction samples\n", 
      _f.getName().c_str(), _index,
      _cycle_samples,
      _inst_samples);
  }
}
