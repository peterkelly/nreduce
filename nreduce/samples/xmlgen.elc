(main ()
(let
 ((printelem  (!indent.!elem.
                cons (indentstr indent)
                (cons "<"
                (cons (ename elem)
                (cons (liststr (map printattr (eattrs elem)))
                (if (echildren elem)
                  (cons ">\n" 
                  (cons (liststr (map (!e2.(liststr (printelem (+ indent 1) e2)))
                                      (echildren elem)))
                  (cons "</"
                  (cons (ename elem)
                  (cons ">\n" nil)))))
                  (cons "/>\n" nil)))))))
  (printattr  (!attr.liststr (cons " "
                            (cons (aname attr)
                            (cons "="
                            (cons "\""
                            (cons (avalue attr)
                            (cons "\"" nil))))))))
  (echildren  (!elem.tail (tail elem)))
  (eattrs     (!elem.head (tail elem)))
  (ename      (!elem.head elem))
  (mkelem     (!name.!attrs.!children.cons name (cons attrs children)))
  (avalue     (!attr.tail attr))
  (aname      (!attr.head attr))
  (mkattr     (!name.!value.cons name value))
  (indentstr  (!indent.if (= indent 0) "" (ap "  " (indentstr (- indent 1)))))
  (liststr    (!lst.if lst (ap (head lst) (liststr (tail lst))) ""))
  (map        (!f.!lst.if lst (cons (f (head lst)) (map f (tail lst))) nil)))

  (printelem
    0
    (mkelem "people" nil
      (cons (mkelem "person" 
        (cons (mkattr "name" "Fred")
        (cons (mkattr "age" "20")
        (cons (mkattr "occupation" "Plumber") nil))) nil)
      (cons (mkelem "person" 
        (cons (mkattr "name" "Joe")
        (cons (mkattr "age" "33")
        (cons (mkattr "occupation" "Marketing executive") nil))) nil)
      (cons (mkelem "person" 
        (cons (mkattr "name" "Bob")
        (cons (mkattr "age" "38")
        (cons (mkattr "occupation" "Manager") nil))) nil) nil)))))))
