#include "runtime.h"
static fun_any_t lam11562;
void lam11562_proc(fun_any_t k11560) {
fun_prim_eq(fun_mk_int(1), fun_mk_int(1), fun_mk_clos(lam11563, k11560));
}
int main() {
lam11562 =  fun_mk_proc((void*)lam11562_proc);
fun_as_proc(lam11562)(halt);
return 0;
}