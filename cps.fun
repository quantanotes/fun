(begin
    (defun cadr  (x) (car (cdr x)))
    (defun caar  (x) (car (car x)))
    (defun caadr (x) (car (cadr x)))

    (defun fun? (exp)
        (and
            (pair? exp)
            (eq (car exp) 'fun)))

    (defun void? (exp) (eq void exp))

    (defun app? (exp)
        (and
            (pair?   exp)
            (symbol? (car exp))))

    (defun aexp? (exp)
        (or
            (atom? exp)
            (fun?  exp)
            (void? exp)))


    
    (cps '(a b)))