(!y.(!map.(!foldl.(!liststr.(!listcount.(!mkrow.(!mkgrid.(!nrows.(!ncols.(!grid.
(!printcell.(!printrow.(!printgrid.(!getitem.


//(printgrid (mkgrid 10 10))


(getitem (cons 1 (cons 2 (cons 3 nil))) 3)


)



(y (!getit.!list.!itemno.if (= itemno 0) (head list)
                            (if (nil? (tail list)) nil
                            (getit getit (tail list) (- itemno 1))))))


/* printgrid */
(!grid.liststr (map printrow grid)))

/* printrow */
(!row.ap (liststr (map printcell row)) "\n"))

/* printcell */
(!cell.if cell "*" " "))

/* grid */
(cons   (cons nil (cons 1 (cons 1   nil)))
 (cons   (cons 1   (cons 1 (cons nil nil)))
  (cons (cons nil (cons 1 (cons nil nil))) nil))))

/* ncols */
(!grid.listcount (head grid)))

/* nrows */
(!grid.listcount grid))

/* mkgrid */
(y (!mkgridr.!rows.!cols.if (= rows 0) nil
                             (cons (mkrow cols) (mkgridr mkgridr (- rows 1) cols)))))

/* mkrow */

(y (!mkrowr.!cols.if (= cols 0) nil (cons 1 (mkrowr mkrowr (- cols 1))))))

/* listcount */
(y (!listcountr.!lst.if lst (+ 1 (listcountr listcountr (tail lst))) 0)))

/* liststr */
(!lst.foldl ap "" lst))

/* foldl */
(y (!foldlr.!f.!base.!lst.if lst (f (head lst) (foldlr foldlr f base (tail lst)))
                                 base)))

/* map */
(y (!mapr.!f.!lst.if lst (cons (f (head lst)) (mapr mapr f (tail lst))) nil)))

(!fun.(!f.f f) fun)
