(begin
    (defun cadr  (x) (car (cdr x)))
    (defun caar  (x) (car (car x)))
    (defun caadr (x) (car (cadr x)))

    (defun void? (exp) (eq void exp))
    (defun fun?  (exp) (and (pair? exp) (eq (car exp) 'fun)))
    (defun app?  (exp) (and (pair? exp) (symbol? (car exp))))
    (defun aexp? (exp) (or (atom? exp) (fun?  exp) (void? exp)))

    (defun m (exp)
        (cond
            (aexp? exp)
            (app?  (m-fun exp))
            (true  'm-error)))
    
    (defun m-fun (exp)
        (let
            ((x (cadr exp))
            (e  (caddr exp))
            ($k (gensym)))
            `(fun ,(x, ,$k) ,(t e $k))))

    (defun t (exp k)
        (cond
            ((aexp? exp) `(,k ,(m exp)))
            ((app? exp)  (t-app exp))
            (true        't-error)))

    (defun t-app (exp k)
        (let
            ((f (car exp))
            (x  (cadr exp))
            ($f (gensym))
            ($x (gensym)))
            (t f `(fun (,$f)
                ,(t x `(fun (,$x)
                    (,$f ,$x ,k)))))))

    (defun cps (exp) (t exp 'halt))

    (cps '(f g)))