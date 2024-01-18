; Bytecode compiler
;
; Instruction set:
;
; push
; pop
; load
; store
; call
; 

(begin
    (def num-constants 0)

    (def bytecode '())

    (def constants '())

    (defun make-instruction (op arg1 arg2)
        ())

    (defun make-constant ())
    
    (defun emit (op, arg1 arg2)
        '(op arg1 arg2))

    (defun bytecoder (x)
        (cond
            ((atom? x) (emit 'push ))
            (true      'bytecoder-error))))