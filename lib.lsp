(label null
  (λ (x) (eq x nil)))

(label map
  (λ (f lst)
    (cond ((null lst) nil)
          ((consp lst)
           (cons (f (car lst))
                 (map f (cdr lst)))))))

(label let (macro (vars . body)
  `(,`(λ ,(map (λ (x) (car x)) vars)
         ,@body) ,@(map (λ (x) (car (cdr x))) vars))))

(label defun
  (macro (name args . exp)
    `(label ,name ,`(λ ,args ,@exp))))

(label defmacro
  (macro (name args . exp)
    `(label ,name ,`(macro ,args ,@exp))))

(label Y
  (λ (f)
    ((λ (x)
       (f (λ (y) ((x x) y))))
     (λ (x)
       (f (λ (y) ((x x) y)))))))

