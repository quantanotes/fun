    (defun m (exp)
        (cond
            ((atom? exp) exp)
            ((fun? exp)  (m-fun exp))
            (true        'm-error)))

    (defun m-fun (exp)
        (let*
            (($k  (gensym))
            (args (caadr exp))
            (body (cadr exp)))
            `(fun (,args ,$k) ,(tc body $k))))
    
    (defun tc (exp c)
        (cond
            ((aexp? exp) `(,c ,(m exp)))
            ((app? exp)  (tc-app exp c))))

    (defun tc-app (exp c)
        (let*
            ((f (car exp))
            (a  (cadr exp)))
            (tk f (fun ($f)
                (t*k a (fun ($a)
                    `(,$f @$a ,c)))))))

    (defun tk (exp k)
        (cond
            ((aexp? exp) (k (m exp)))
            ((app? exp)  (tk-app exp))))

    (defun tk-app (exp, k)
        (let*
            ((f   (car exp))
            (a    (cadr exp))
            ($rv  (gensym))
            (cont `(fun (,$rv) ,(k $rv))))
            ()))

    (defun t*k (exps k)
        (cond
            ((nil? exps) (k '()))
            ((pair? exps)
                (tk (car exps) (fun (head)
                    (t*k (cdr exps) (fun (tail)
                        (k (cons head tail)))))))))

    (defun cps (exp) (tc exp 'halt))


    tk f (fun ($f)
                (tk x (fun ($x)
                    `(,$f ,$x ,c))))