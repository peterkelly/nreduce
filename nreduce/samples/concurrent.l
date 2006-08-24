(len1       (lst total) (if (and total lst) (len1 (tail lst) (+ total 1)) total))
(len        (lst) (let ((alen (arrayoptlen lst)))
                          (if (alen)
                              alen
                              (len1 lst 0))))
(item1      (n lst) (if (and lst (= n 0)) (head lst) (item1 (- n 1) (tail lst))))
(item       (n lst) (if (arrayhas n lst)
                        (arrayitem n lst)
                        (item1 (arrayremsize lst n) (arrayrem lst))))
(range      (min max) (if (= min max) (cons min nil) (cons min (range (+ min 1) max))))

(map (f lst)
(if lst
    (cons (f (head lst))
          (map f (tail lst)))
    nil))

(filter (f lst)
(if lst
    (if (f (head lst))
        (cons (head lst) (filter f (tail lst)))
        (filter f (tail lst)))
    nil))

(append (a b)
(if a
    (cons (head a) (append (tail a) b))
    b))

(printlist (lst)
(if (not lst)
    nil
    (cons (head lst) (cons "\n" (printlist (tail lst))))))



(output (val cont) (cons (cons "output" (cons val nil)) cont))
(send (dest val cont) (cons (cons "send" (cons dest (cons val nil))) cont))
(done (pid cont) (cons (cons "done" (cons pid nil)) cont))

(replace (lst n pos val)
(if (not lst)
    nil
    (if (= n pos)
        (cons val (tail lst))
        (cons (head lst) (replace (tail lst) (+ n 1) pos val)))))

(outputs (lst)
(map (item 1) (filter (!msg.(= (item 0 msg) "output")) lst)))

(messagesfor (lst process)
(map (item 2)
(filter (!msg.if (= (item 0 msg) "send")
                 (if (= (item 1 msg) process)
                     1
                     nil)
                 nil)
        lst)))


(control (stream procs)
(if (not stream)
    nil
    (let ((msg (head stream))
          (rest (tail stream)))
         (if (= (item 0 msg) "send")
             (let ((dst (item 1 msg))
                   (val (item 2 msg))
                   (res (item dst procs))
                   (newmsgs (head res))
                   (newproc (tail res)))
                  (append newmsgs (control rest (replace procs 0 dst newproc))))
         (if (= (item 0 msg) "output")
             (control rest procs)
         (if (= (item 0 msg) "done")
             (output (cons "done: " (item 1 msg)) nil)
             nil))))))

(runprocs (pfuns)
(let ((stream (send 0 "x" (control stream procs)))
      (procs (map (!pid.(item pid pfuns) (messagesfor stream pid))
             (range 0 (len pfuns)))))
     (printlist (outputs stream))))














(proc0a (n) (if (= n 0) (output "i am done" (done 0 nil))
  (output n
  (send 1 (+ n 2)
  (send 2 (+ n 4)
  (proc0a (- n 1)))))))

(proc0 (stream)
(cons (proc0a 10) nil))

(proc1 (stream) (if (not stream) (done 1 nil)
  (cons (output (cons "proc1 received " (cons (head stream) nil)) nil)
  (proc1 (tail stream)))))

(proc2 (stream) (if (not stream) (done 2 nil)
  (cons (output (cons "proc2 received " (cons (head stream) nil)) nil)
  (proc2 (tail stream)))))


(main () (runprocs (cons proc0 (cons proc1 (cons proc2 nil)))))
