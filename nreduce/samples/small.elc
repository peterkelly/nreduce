mandel Zr Zi Cr Ci iterations res =
  (if (= res iterations)
      res
      (letrec newZr = (+ (- (* Zr Zr) (* Zi Zi)) Cr)
              newZi = (+ (* 2.0 (* Zr Zi)) Ci)
              mag = (sqrt (+ (* newZr newZr) (* newZi newZi)))
       in
           (if (> mag 2.0)
               res
               (mandel newZr newZi Cr Ci iterations (+ res 1)))))

mandel0 Cr Ci iterations = (mandel 0.0 0.0 Cr Ci iterations 0)

printcell num = (if (> num 40) "  "
                (if (> num 30) "''"
                (if (> num 20) "--"
                (if (> num 10) "**" "##"))))

mloop minx maxx xincr miny maxy yincr x y =
(if (> x maxx)
    (if (> y maxy)
        (cons nil nil)
        (cons nil (mloop minx maxx xincr miny maxy yincr minx (+ y yincr))))
    (cons (mandel0 x y 64)
          (mloop minx maxx xincr miny maxy yincr (+ x xincr) y)))

printmand lst =
(if lst
    (if (head lst)
        (append (printcell (head lst)) (printmand (tail lst)))
        (append "\n" (printmand (tail lst))))
    nil)

mandelrange minx maxx xincr miny maxy yincr =
(letrec
   lst = (mloop minx (+ maxx xincr) xincr miny maxy yincr minx miny)
 in
   (seq (sparklist lst) (printmand lst)))

//main = (mandelrange -1.5 0.5 0.046 -1.0 1.0 0.046)
main = (mandelrange 0.06625 0.44125 0.017222 0.29625 0.67125 0.017222)
//main = (mandelrange -0.321875 0.053125 0.008611 -0.980625 -0.605625 0.008611)
