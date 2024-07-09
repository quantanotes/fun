#ifndef FUN_PRINT_H
#define FUN_PRINT_H

#include "any.h"
#include <stdio.h>

static void fun_print(fun_any_t x) {
  switch (x.tag) {
  case FUN_TAG_PTR:
    printf("ptr<%p>", fun_as_obj(x));
    break;
  case FUN_TAG_INT:
    printf("int<%d>", fun_as_int(x));
    break;
  }
}

#endif // FUN_PRINT_H