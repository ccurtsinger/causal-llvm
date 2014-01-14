#include <stdio.h>

#include <causal.h>

void a();
void b();
void c();

void a() {
  for(int i=0; i<100; i++) {
    __asm__("pause");
    c();
  }
}

void b() {
  for(int i=0; i<100; i++) {
    __asm__("pause");
  }
}

void c() {
  for(int i=0; i<100; i++) {
    __asm__("pause");
  }
}

int main(int argc, char** argv) {
  for(int i=0; i<1000000; i++) {
    a();
    b();
    CAUSAL_PROGRESS;
  }
  
  return 0;
}
