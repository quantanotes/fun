(begin
    (defun capture ()
        (let ((x 42))
            (fun () outer)))
            
    (def outer 45)
    (def captured (capture))
    
    (println (captured)))