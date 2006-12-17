testfun a b = (* a b) /* This is the one that should be referenced */

f x =
(append "item: "
        (append (numtostring (testfun x 10))
                "\n"))
