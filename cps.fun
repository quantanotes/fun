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
            (atom? exp)
            (fun?  (m-fun exp))
            (true  'm-error)))
    
    (defun m-fun (exp)
        (let
            ((x (cdr exp))
            (e  (caddr exp))
            ($k (gensym)))
            `(fun (@x ,$k) ,(tc e $k))))

    (defun tc (exp c)
        (cond
            ((aexp? exp) `(,c ,(m exp)))
            ((app? exp)  (tc-app exp))
            (true        'tc-error)))

    (defun tc-app (exp c)
        (let
            ((f (car exp))
            (x  (cdr exp))
            ($f (gensym))
            ($x (gensym)))
            (tk f (fun ($f)
                (t*k x (fun ($x)
                    `(,$f @$x ,c)))))))

    (defun tk (exp k)
        (cond
            ((aexp? exp) (k (m exp)))
            ((app? exp)  (tk-app exp))
            (true        'tk-error)))

    (defun tk-app (exp k)
        (let*
            (($r gensym)
            (c `(fun (,$r) ,(k $r))))
            (tc exp c)))

    (defun t*k (exps k)
        (println 'exps: exps)
        (println 'k: k)
        (println 'k-app: (k '()))
        (println 'nil?: (nil? exps))
        (cond
            ((nil? exps)  (k '()))
            ((pair? exps) (tk (car exps) (fun (hd)
                (t*k (cdr exps) (fun (tl) (k (cons hd tl)))))))
            (true         (k '()))))

    (defun cps (exp) (tc exp 'halt))
    
    (cps '(f g)))