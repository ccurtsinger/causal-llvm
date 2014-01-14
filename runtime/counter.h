#if !defined(CAUSAL_RUNTIME_COUNTER_H)
#define CAUSAL_RUNTIME_COUNTER_H

class Counter {
private:
  const char* _file;
  int _line;
  volatile size_t* _ctr;
public:
  Counter(const char* file, int line, volatile size_t* ctr) : 
    _file(file), _line(line), _ctr(ctr) {}
  
  const char* getFile() const { return _file; }
  int getLine() const { return _line; }
  
  size_t getValue() const {
    return *_ctr;
  }
};

#endif
