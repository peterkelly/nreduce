super (map f lst) (
  if lst
     (cons (f (head lst)) (map f (tail lst)))
     nil
)

super (len lst) (
  if lst
     (+ 1 (len (tail lst)))
     0
)

super (item n lst) (
  if (= n 0)
     (head lst)
     (item (- n 1) (tail lst))
)

super (range min max) (
  if (= min max)
     (cons min nil)
     (cons min (range (+min 1) max))
)

super (printcell cell) (
  if (= cell 1)
     "##"
     "--"
)

super (printrow row) (
  if row
    (cons (printcell (head row)) (printrow (tail row)))
    "\n"
)

super (printgrid grid) (
  map printrow grid
)

super (grows grid) (
  len grid
)

super (gcols grid) (
  len (head grid)
)

super (gcell grid row col) (
  item col (item row grid)
)

super (mkrow pat rows cols startrow endrow startcol endcol row) (
  map (mkcol pat rows cols startrow endrow startcol endcol row) (range 0 (- cols 1))
)

super (mkcol pat rows cols startrow endrow startcol endcol row col) (
  if (&& (>= row startrow) (&& (< row endrow) (&& (>= col startcol) (< col endcol))))
     (gcell pat (- row startrow) (- col startcol))
     0
)

super (makegrid pat rows cols) (
  letrec
    startrow  (/ (- rows (grows pat)) 2)
    endrow    (+ startrow (grows pat))
    startcol  (/ (- cols (gcols pat)) 2)
    endcol    (+ startcol (gcols pat))
  in
    map (mkrow pat rows cols startrow endrow startcol endcol) (range 0 (- rows 1))
)

super (dorow grid nrows ncols row) (
  map (docol grid nrows ncols row) (range 0 (- ncols 1))
)

super (docol grid nrows ncols row col) (
  letrec
    curvalue (gcell grid row col)
    count    (sumnb grid row col)
  in
    if (&& (= curvalue 0) (= count 3))
       1
       (if (&& (= curvalue 1) (|| (= count 2) (= count 3)))
           1
           0)
)

super (dolife grid) (
  letrec
    nrows (grows grid)
    ncols (gcols grid)
  in
    map (dorow grid nrows ncols) (range 0 (- nrows 1))
)

super (sumnb g row col) (
  letrec
    maxrow  (- (grows g) 1)
    maxcol  (- (gcols g) 1)
  in
    (+ (if (&& (> row 0) (> col 0))        (gcell g (- row 1) (- col 1)) 0)
    (+ (if (> row 0)                       (gcell g (- row 1) col)       0)
    (+ (if (&& (> row 0) (< col maxcol))   (gcell g (- row 1) (+ col 1)) 0)
    (+ (if (> col 0)                       (gcell g row       (- col 1)) 0)
    (+ (if (< col maxcol)                  (gcell g row       (+ col 1)) 0)
    (+ (if (&& (< row maxrow) (> col 0))   (gcell g (+ row 1) (- col 1)) 0)
    (+ (if (< row maxrow)                  (gcell g (+ row 1) col)       0)
    (if (&& (< row maxrow) (< col maxcol)) (gcell g (+ row 1) (+ col 1)) 0))))))))
)

super (loop iter grid) (
  if (> iter 0)
     (cons (printgrid grid) (cons "\n" (loop (- iter 1) (dolife grid))))
     nil
)

letrec start  (cons (cons 0 (cons 1 (cons 1 nil)))
              (cons (cons 1 (cons 1 (cons 0 nil)))
              (cons (cons 0 (cons 1 (cons 0 nil))) nil)))

in
  loop 50 (makegrid start 20 20)
