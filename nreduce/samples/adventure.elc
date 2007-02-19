parse stream start count =
(if stream
    (if (or (= (head stream) '\n') (= (head stream) '\r'))
        (if (> count 0)
            (cons (prefix count start)
                  (parse (tail stream) (tail stream) 0))
            (parse (tail stream) (tail stream) 0))
        (parse (tail stream) start (+ count 1)))
    nil)

default cmd rest room =
(if (or (streq cmd "i") (streq cmd "inventory"))
    (append "You have nothing.\n" (prompt rest room))
(if (or (streq cmd "h") (streq cmd "help"))
    (append "There is no help for this game!\n" (prompt rest room))
(append "Unknown command: \"" (append cmd (append "\"\n" (prompt rest room))))))

prompt commands room =
(append "\n> "
(if commands
    (letrec
       cmd = (head commands)
       rest = (tail commands)
     in
       (room cmd rest))
    nil))

gotoroom rest room =
(room "look" rest)

entrance cmd rest =
(if (streq cmd "look")
    (append "You are in a strange room. There is a door to the north and a passage"
      (append " to the south.\n"
      (prompt rest entrance)))
(if (streq cmd "look door")
    (append "It is closed.\n"
      (prompt rest entrance))
(if (streq cmd "open door")
    (append "It appears to be locked.\n"
      (prompt rest entrance))
(if (or (streq cmd "s") (streq cmd "south"))
    (gotoroom rest passage)
    (default cmd rest entrance)))))

passage cmd rest =
(if (streq cmd "look")
    (append "You are in a short passageway. The only entrance is to the north.\n\n"
      (append "There is a terrible smell coming from the floor.\n"
      (prompt rest passage)))
(if (streq cmd "smell")
    (append "You detect a whiff of sewage coming from below. There appearns to be a manhole\n"
      (append "cover on the ground from which the smell is emanating.\n"
      (prompt rest passage)))
(if (or (streq cmd "open manhole cover") (streq cmd "open cover"))
    (append "Do you *really* want to do that? I don't think so.\n"
      (prompt rest passage))
(if (or (streq cmd "n") (streq cmd "north"))
    (gotoroom rest entrance)
    (default cmd rest passage)))))

process stream =
(append "Welcome to nreduce adventure. Enjoy!\n\n"
(gotoroom (parse stream stream 0) entrance))

xmain =
(letrec
   con = (connect "localhost" 3005 (process con))
 in
   con)

ymain = (seq (listen "0.0.0.0" 1234 process) nil)




showdata connections n =
(if connections
    (append "Connection "
      (append (numtostring n)
      (append " sent the following data:\n"
      (append (head connections)
      (append "\n"
              (showdata (tail connections) (+ n 1)))))))
    "End of connection data\n")

zmain =
(letrec
   connections = (listen "0.0.0.0" 1234 process)
 in
   (showdata connections 0))





parconnections lst =
(if lst
    (par (head lst) (parconnections (tail lst)))
    nil)

main = (parconnections (listen "0.0.0.0" 1234 process))
