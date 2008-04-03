printlist list=
          (if list
              (append (head list)
                      (cons " "
                             (printlist (tail list))))
               nil)

printstrings lst =
                 (if lst
                     (append (printlist (head lst)) 
                             (cons '\n' 
                                   (printstrings (tail lst))))
                     nil)


main = (printstrings (zzip_read_dirent "/home/smaxll/Project/workspace/zipc/test.zip"))
