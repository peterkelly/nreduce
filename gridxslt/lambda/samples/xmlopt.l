(!y.(!map.(!liststr.(!indentstr.
(!mkattr.!aname.!avalue.!mkelem.!ename.!eattrs.!echildren.
(!printattr.
(!printelem.

(liststr (printelem 0
  (mkelem "people" nil
  (cons (mkelem "person" (cons (mkattr "name" "Fred") (cons (mkattr "age" "20")
            (cons (mkattr "occupation" "Plumber") nil))) nil)
  (cons (mkelem "person" (cons (mkattr "name" "Joe") (cons (mkattr "age" "33")
            (cons (mkattr "occupation" "Marketing executive") nil))) nil)
  (cons (mkelem "person" (cons (mkattr "name" "Bob") (cons (mkattr "age" "38")
            (cons (mkattr "occupation" "Manager") nil))) nil) nil))))))
)

/* printelem */

(y (!printelemr.!indent.!elem.
       cons (indentstr indent) (cons "<" (cons (ename elem)
       (cons (liststr (map printattr (eattrs elem)))
       (if (echildren elem)
         (cons ">\n" (cons (liststr (map (!e2.(liststr (printelemr printelemr (+ indent 1) e2))) (echildren elem))) 
(cons "</" (cons (ename elem) (cons ">\n" nil)))
))
         (cons "/>\n" nil))))))))

/* printattr */

(!attr.liststr (cons " " (cons (aname attr) (cons "="
               (cons "\"" (cons (avalue attr) (cons "\"" nil))))))))


/* mkattr */      (!name.!value.cons name value)
/* aname */       (!attr.head attr)
/* avalue */      (!attr.tail attr)

/* mkelem */      (!name.!attrs.!children.
                     (cons name (cons attrs children)))
/* ename */       (!elem.head elem)
/* eattrs */      (!elem.head (tail elem))
/* echildren */   (!elem.tail (tail elem))

)

(y (!indentstrr.!indent.
    if (= indent 0) "" (ap "  " (indentstrr indentstrr (- indent 1))))))

/* liststr */

(y (!self.!lst.if lst (ap (head lst) (self self (tail lst))) "")))

/* map */

(y (!mapr.!f.!lst.if lst (cons (f (head lst)) (mapr mapr f (tail lst))) nil)))

/* y */
(!fun.(!f.f f) fun)
