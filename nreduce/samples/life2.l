(main ()
(let
  ((map        (!f.!lst.if lst (cons (f (head lst)) (map f (tail lst))) nil))
   (foreach    (!lst.!f.map f lst))
   (reduce     (!f.!init.!lst.if lst (f (head lst) (reduce f init (tail lst))) init))
   (for        (!min.!max.!f.map f (range min max)))

   (len        (!lst.if lst (+ 1 (len (tail lst))) 0))
   (item       (!n.!lst.if (= n 0) (head lst) (item (- n 1) (tail lst))))
   (range      (!min.!max.if (= min max) (cons min nil) (cons min (range (+ min 1) max))))

   (sum        (!lst.reduce + 0 lst))
   (product    (!lst.reduce * 1 lst))

   (creategrid (!rows.!cols.map (!n.map (!n.0) (range 1 rows)) (range 1 cols)))
   (printcell  (!cell.if (= cell 1) "##" "--"))
   (printrow   (!row.if row (cons (printcell (head row)) (printrow (tail row))) "\n"))
   (printgrid  (!grid.map printrow grid))
   (grows      (!grid.len grid))
   (gcols      (!grid.len (head grid)))
   (gcell      (!grid.!row.!col.item col (item row grid)))

   (makegrid   (!pat.!rows.!cols.
                (let ((startrow  (floor (/ (- rows (grows pat)) 2)))
                      (endrow    (+ startrow (grows pat)))
                      (startcol  (floor (/ (- cols (gcols pat)) 2)))
                      (endcol    (+ startcol (gcols pat))))
                 (for 0 (- rows 1) (!row.
                   (for 0 (- cols 1) (!col.
                     (if (and (>= row startrow) (and (< row endrow)
                         (and (>= col startcol) (< col endcol))))
                       (gcell pat (- row startrow) (- col startcol)) 0))))))))
   (dolife     (!grid.
                (let ((nrows (grows grid))
                      (ncols (gcols grid)))
                  (for 0 (- nrows 1) (!row.
                     for 0 (- ncols 1) (!col.
                       (let
                         ((curvalue  (gcell grid row col))
                          (count     (sumnb grid row col)))
                         (if (and (= curvalue 0) (= count 3))
                             1
                         (if (and (= curvalue 1) (or (= count 2) (= count 3)))
                             1
                             0)))))))))
   (sumnb      (!g.!row.!col.
                (let
                  ((maxrow  (- (grows g) 1))
                   (maxcol  (- (gcols g) 1)))
                ((+ (if (and (> row 0) (> col 0))        (gcell g (- row 1) (- col 1)) 0)
                (+ (if (> row 0)                       (gcell g (- row 1) col)       0)
                (+ (if (and (> row 0) (< col maxcol))   (gcell g (- row 1) (+ col 1)) 0)
                (+ (if (> col 0)                       (gcell g row       (- col 1)) 0)
                (+ (if (< col maxcol)                  (gcell g row       (+ col 1)) 0)
                (+ (if (and (< row maxrow) (> col 0))   (gcell g (+ row 1) (- col 1)) 0)
                (+ (if (< row maxrow)                  (gcell g (+ row 1) col)       0)
                (if (and (< row maxrow) (< col maxcol)) (gcell g (+ row 1) (+ col 1)) 0))))))))))))
   (start      (cons (cons 0 (cons 1 (cons 1 nil)))
               (cons (cons 1 (cons 1 (cons 0 nil)))
               (cons (cons 0 (cons 1 (cons 0 nil))) nil))))
   (loop       (!iter.!grid.
                if (> iter 0)
                (cons (printgrid grid) (cons "\n" (loop (- iter 1) (dolife grid))))
                nil)))

(loop 20 (makegrid start 8 8))))
