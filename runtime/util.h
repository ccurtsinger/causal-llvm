#if !defined(CAUSAL_RUNTIME_UTIL_H)
#define CAUSAL_RUNTIME_UTIL_H

#include <time.h>

enum Time {
  Time_ns = 1,
  Time_us = 1000 * Time_ns,
  Time_ms = 1000 * Time_us,
  Time_s = 1000 * Time_ms
};

static size_t getTime() {
  struct timespec ts;
  if(clock_gettime(CLOCK_REALTIME, &ts)) {
    perror("getTime():");
    abort();
  }
  return ts.tv_nsec + ts.tv_sec * Time_s;
}

template<class T> class wrapped_array {
private:
  T* _base;
  size_t _size;
public:
  // Construct an array wrapper from a base pointer and array size
  wrapped_array(T* base, size_t size) : _base(base), _size(size) {}
  wrapped_array(const wrapped_array& other) : _base(other._base), _size(other._size) {}
  
  // Get the size of the wrapped array
  size_t size() { return _size; }
  
  // Access an element by index
  T& operator[](size_t i) { return _base[i]; }
  
  // Get a slice of this array, from a start index (inclusive) to end index (exclusive)
  wrapped_array<T> slice(size_t start, size_t end) {
    return wrapped_array<T>(&_base[start], end - start);
  }
  
  // Iterator class for convenient range-based for loop support
  class iterator {
  private:
    T* _p;
  public:
    // Start the iterator at a given pointer
    iterator(T* p) : _p(p) {}
    
    // Advance to the next element
    void operator++() { ++_p; }
    void operator++(int) { _p++; }
    
    // Get the current element
    T& operator*() { return *_p; }
    
    // Compare iterators
    bool operator==(const iterator& other) { return _p == other._p; }
    bool operator!=(const iterator& other) { return _p != other._p; }
  };
  
  // Get an iterator positioned at the beginning of the wrapped array
  iterator begin() { return iterator(_base); }
  
  // Get an iterator positioned at the end of the wrapped array
  iterator end() { return iterator(&_base[_size]); }
};

// Function for automatic template argument deduction
template<class A> wrapped_array<A> wrap(A* base, size_t size) {
  return wrapped_array<A>(base, size);
}

template<class C, class F> class filterer {
private:
  C _collection;
  F _fn;
public:
  filterer(C collection, F fn) : _collection(collection), _fn(fn) {}
  
  class iterator {
  private:
    C& _source;
    typename C::iterator _pos;
    F _fn;
    
    void skipFiltered() {
      while(_pos != _source.end() && !_fn(*_pos)) { _pos++; }
    }
  public:
    iterator(C& source, typename C::iterator pos, F fn) : _source(source), _pos(pos), _fn(fn) { 
      skipFiltered();
    }
    
    void operator++() {
      _pos++;
      skipFiltered();
    }
    
    auto operator*() -> decltype(*_pos) {
      return *_pos;
    }
    
    bool operator==(const iterator& other) {
      return _pos == other._pos;
    }
    
    bool operator!=(const iterator& other) {
      return _pos != other._pos;
    }
  };
  
  iterator begin() {
    return iterator(_collection, _collection.begin(), _fn);
  }
  
  iterator end() {
    return iterator(_collection, _collection.end(), _fn);
  }
};

template<class C, class F> filterer<C, F> filter(C&& collection, F fn) {
  return filterer<C, F>(std::move(collection), fn);
}

template<class C, class F> filterer<C&, F> filter(C& collection, F fn) {
  return filterer<C&, F>(collection, fn);
}

#endif
