#|
Bytecode Compiler

Assumes CPS transform

Instruction set:

const
local
global
define
apply
cont
|#

(begin
    (import "fun/cps.fun")
    (import "fun/ff.fun")

    (cc '(fun (a) (a b c))))