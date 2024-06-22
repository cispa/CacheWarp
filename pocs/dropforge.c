#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int math(int a, int b) {
  int ret = 0;
  ret += a * 10 + b;

  return ret;
}

int main() {
    int ret, reset;
    while (1) {
    	do {
        ret = math(1, 1);
        reset = math(2, 5);
      }
      while( ret == 11);
      printf("%d\n", ret);
    }
    return 0;
}