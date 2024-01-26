#|
Bytecode Compiler

Assumes CPS transform

Instruction set:

const
local
global
define
apply
callcc
|#

(begin
    (import "fun/cps.fun")
    (import "fun/ff.fun")

    (defun transform (x) (ff (cps x)))

    (def exp '(fun (x) (fun (y) (z x y))))

    (cps exp))