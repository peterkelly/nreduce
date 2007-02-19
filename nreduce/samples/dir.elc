printlist lst =
(if (not lst)
    nil
    (append "entry: name = "
    (append (item 0 (head lst))
    (append ", type = "
    (append (item 1 (head lst))
    (append ", size = "
    (append (numtostring (item 2 (head lst)))
    (append "\n"
    (printlist (tail lst))))))))))

main = (printlist (readdir "/etc"))

