(mandel (Zr Zi Cr Ci iterations res)
  (if (= res iterations)
      res
      (let ((newZr (+ (- (* Zr Zr) (* Zi Zi)) Cr))
            (newZi (+ (* 2.0 (* Zr Zi)) Ci))
            (mag (sqrt (+ (* newZr newZr) (* newZi newZi)))))
           (if (> mag 2.0)
               res
               (mandel newZr newZi Cr Ci iterations (+ res 1))))))

(mandel0 (Cr Ci iterations) (mandel 0.0 0.0 Cr Ci iterations 0))

(map    (f lst) (if lst (cons (f (head lst)) (map f (tail lst))) nil))
(for    (min max incr f) (map f (range min max incr)))
(range  (min max incr) (if (>= min max) (cons min nil) (cons min (range (+ min incr) max incr))))

(printcell (num) (if (> num 40) "  "
                 (if (> num 30) "''"
                 (if (> num 20) "--"
                 (if (> num 10) "**" "##")))))

(mloop (minx maxx xincr miny maxy yincr x y)
(if (> x maxx)
    (if (> y maxy)
        (append "\n" nil)
        (append "\n" (mloop minx maxx xincr miny maxy yincr minx (+ y yincr))))
    (append (printcell (mandel0 x y 1024))
          (mloop minx maxx xincr miny maxy yincr (+ x xincr) y))))

(mandelrange (minx maxx xincr miny maxy yincr)
(mloop minx (+ maxx xincr) xincr miny maxy yincr minx miny))

//(main () (mandelrange -1.5 0.5 0.046 -1.0 1.0 0.046))
(main () (mandelrange 0.06625 0.44125 0.008611 0.29625 0.67125 0.008611))
//(main () (mandelrange -0.321875 0.053125 0.008611 -0.980625 -0.605625 0.008611))
