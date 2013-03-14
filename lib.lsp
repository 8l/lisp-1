(label caar (λ (x) (car (car x))))
(label cddr (λ (x) (cdr (cdr x))))
(label cadr (λ (x) (car (cdr x))))
(label cdar (λ (x) (cdr (car x))))
(label caaar (λ (x) (car (car (car x)))))
(label cddar (λ (x) (cdr (cdr (car x)))))
(label cadar (λ (x) (car (cdr (car x)))))
(label cdaar (λ (x) (cdr (car (car x)))))
(label caadr (λ (x) (car (car (cdr x)))))
(label cdddr (λ (x) (cdr (cdr (cdr x)))))
(label caddr (λ (x) (car (cdr (cdr x)))))
(label cdadr (λ (x) (cdr (car (cdr x)))))

(label null
  (λ (x) (eq x nil)))

(label map
  (λ (f lst)
    (cond ((null lst) nil)
          ((consp lst)
           (cons (f (car lst))
                 (map f (cdr lst)))))))

(label let (μ (vars . body)
  `((λ ,(map car vars)
      ,@body)
    ,@(map cadr vars))))

(label defun
  (μ (name args . exp)
    `(label ,name (λ ,args ,@exp))))

(label defmacro
  (μ (name args . exp)
    `(label ,name (μ ,args ,@exp))))

(label Y
  (λ (f)
    ((λ (x)
       (f (λ (y) ((x x) y))))
     (λ (x)
       (f (λ (y) ((x x) y)))))))

