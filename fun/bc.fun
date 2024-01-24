#| Bytecode Compiler

Assumes CPS transform

Instruction set:
const
local
global
define
apply
cont 
#|

(begin
    (import "fun/cps.fun")
    (import "fun/cc.fun")

    (defun make-ctx ()
        ())

    (defun ctx-locals
        ())

    (defun ctx-globals
        ())

    (defun make-const (const ctx)
        ())

    (defun bytecode (exp ctx)
        (set! exp (ll (cps exp)))
        (cond
            ((atom? exp)  (bytecode-atom exp ctx))
            ((apply? exp) (bytecode-apply exp ctx))
            ((fun? exp)   (bytecode-fun exp ctx))
            ((cont? exp)  (bytecode-cont exp ctx))
            (true 'error)))

    (defun bytecode-atom (exp ctx)
        (cond
            (number? )))

    (defun bytecode-apply (exp ctx)
        ())

    (defun bytecode-fun (exp ctx)
        ())

    (defun bytecode-cont (exp ctx)
        ())
    )