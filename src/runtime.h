#ifndef FUN_RUNTIME_H
#define FUN_RUNTIME_H

#include "any.h"
#include "prim.h"
#include "print.h"

static void halt(fun_any_t x) {
  fun_print(x);
  exit(0);
}

#endif // FUN_RUNTIME_H