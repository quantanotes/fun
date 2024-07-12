#ifndef FUN_RUNTIME_H
#define FUN_RUNTIME_H

#include "any.h"
#include "prim.h"
#include "print.h"

static void _halt(fun_any_t x) {
  fun_print(x);
  exit(0);
}

static fun_any_t halt = fun_mk_proc((fun_proc_t)_halt);

#endif // FUN_RUNTIME_H