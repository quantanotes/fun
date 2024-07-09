#include "runtime.h"
int main() {
  fun_prim_add(fun_mk_int(1), fun_mk_int(2), fun_mk_proc((void*)halt));
  return 0;
}