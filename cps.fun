(begin
    (defun cddr  (x) (cdr (cdr x)))
    (defun cadr  (x) (car (cdr x)))
    (defun caadr (x) (car (cadr x)))
    (defun caddr (x) (car (cddr x)))

    (defun fun?   (x) (and (pair? x) (eq (car x) 'fun)))
    (defun unary? (x) (and (pair? x) (eq (length x) 2)))
    (defun aexp?  (x) (or (atom? x) (fun? x)))

    (defun fun-args (x) (caadr x))
    (defun fun-body (x) (caddr x))

    (defun unary-fun (x) (car x))
    (defun unary-arg (x) (cadr x))

    (defun m (x)
        (cond
            ((atom? x) x)
            ((fun?  x) (m-fun x))))

    (defun m-fun (x)
        (let
            ((a  (fun-args x))
             (b  (fun-body x))
             ($k (gensym)))
             `(fun (,a ,$k) ,(tc b $k))))

    (defun tc (x c) 
        (cond
            ((aexp?  x) `(,c ,(m x)))
            ((unary? x) (tc-unary x c))))

    (defun tc-unary (x c)
        (let
            ((f  (unary-fun x))
             (a  (unary-arg x))
             ($f (gensym))
             ($a (gensym)))
             (tk f (fun ($f)
                (tk a (fun ($a)
                    `(,$f ,$a ,c)))))))

    (defun tk (x k)
        (cond
            ((aexp?  x) (k (m x)))
            ((unary? x) (tk-unary x k))))

    (defun tk-unary (x k)
        (let*
            ((f  (unary-fun x))
             (a  (unary-arg x))
             ($r (gensym))
             ($c `(fun (,$r) ,(k $r))))
             (tk f (fun ($f)
                (tk a (fun $a)
                    `(,$f ,$a ,c))))))

    (defun t*k (xs k)
        (cond
            ((nil? xs) (k '()))
            ((pair? xs) (tk (car xs) (fun (hd)
                (t*k (cdr xs) (fun (tl)
                    (k (cons hd tl)))))))))

    (defun cps (x)
        (cond
            ((aexp? x) (m x))
            (true      (tc x 'halt))))
    
    3)