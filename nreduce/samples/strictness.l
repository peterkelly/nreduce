super (cond1 p q r) (if (= p 0) (+ q r) (+ q p))
super (cond2 p q r) (if (= p 0) (+ 1 r) (+ q p))
super (cond3 p q r) (if (= p 0) (+ q r) (+ 1 p))
super (apvar p q r) (p q r)
super (a x y) x
super (b x y) y
super (c x y) (+ x 1)
super (d x y) (+ y 1)
super (e x y) (+ x y)
super (g m n) (e m n)

1
