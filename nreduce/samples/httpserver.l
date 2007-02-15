parse_error name stream fun = (append name (append "Parse error: " (append name "\n")))

getlines stream start count =
(if stream
    (if (= (head stream) '\n')
        // encounter \n - end of line
        (letrec
           rest = (tail stream)
           line = (prefix count start)
         in
           (cons line (getlines rest rest 0)))
    (if (if (= (head stream) '\r') (if (tail stream) (= (head (tail stream)) '\n') nil) nil)
        // encounter \r\n - end of line (but skip 1 more char)
        (letrec
           rest = (tail (tail stream))
           line = (prefix count start)
         in
           (cons line (getlines rest rest 0)))
        // still part of line
        (getlines (tail stream) start (+ count 1))))
    // no more data
    (cons (prefix count start) nil))

gotfirstline method uri version fun =
(fun method uri version)

parse_version method uri stream start count =
(if stream
    (parse_version method uri (tail stream) start (+ count 1))
    (gotfirstline method uri (prefix count start)))

parse_version_start method uri stream start count =
(if stream
    (if (= (head stream) ' ')
        (parse_version_start method uri (tail stream) (tail stream) 0)
        (parse_version method uri stream stream 0))
    (parse_error "version_start" stream))

parse_uri method stream start count =
(if stream
    (if (= (head stream) ' ')
        (parse_version_start method (prefix count start) (tail stream) (tail stream) 0)
        (parse_uri method (tail stream) start (+ count 1)))
    (parse_error "uri" stream))

parse_uri_start method stream start count =
(if stream
    (if (= (head stream) ' ')
        (parse_uri_start method (tail stream) (tail stream) 0)
        (parse_uri method stream stream 0))
    (parse_error "uri_start" stream))

parse_method stream start count =
(if stream
    (if (= (head stream) ' ')
        (parse_uri_start (prefix count start) (tail stream) (tail stream) count)
        (parse_method (tail stream) start (+ count 1)))
    (!x.nil))


httpheader =
(append "HTTP/1.1 200 OK\r\n"
(append "Content-Type: text/plain; charset=UTF-8\r\n"
(append "Connection: close\r\n"
        "\r\n")))

xprocess method uri version =
(append "method = \"" (append method
(append "\", uri = \"" (append uri
(append "\", version = \"" (append version "\"\n"))))))

print_dirs lst =
(if lst
    (if (streq (item 1 (head lst)) "directory")
        (append "<li><a href=\""
          (append (head (head lst))
          (append "/\">"
          (append (head (head lst))
          (append "</a></li>\n" (print_dirs (tail lst)))))))
        (print_dirs (tail lst)))
    nil)

print_files lst =
(if lst
    (if (streq (item 1 (head lst)) "file")
        (append "<li><a href=\""
          (append (head (head lst))
          (append "\">"
          (append (head (head lst))
          (append "</a> "
          (append (numtostring (item 2 (head lst)))
          (append "</li>\n" (print_files (tail lst)))))))))
        (print_files (tail lst)))
    nil)

handle_dir uri =
(append "HTTP/1.1 200 OK\r\n"
(append "Content-Type: text/html; charset=UTF-8\r\n"
(append "Connection: close\r\n"
(append "\r\n"
(append "<html><body><h1>Contents of "
(append uri
(append "</h1>Directories<ul>"
(append (print_dirs (readdir uri))
(append "</ul>Files<ul>"
(append (print_files (readdir uri))
(append "</ul>End</body>" nil)))))))))))

process method uri version =
(if (not (fexists uri))
    (append httpheader "File does not exist")
(if (isdir uri)
    (handle_dir uri)
    (append httpheader (readb uri))))

readrest lst = (if lst (readrest (tail lst)) (printer nil "done"))

parse stream =
(letrec
   lines = (getlines stream stream 0)
   first = (head lines)
 in
  ((parse_method first first 0) process))

handler stream = (parse stream)

main = (parlist (listen "0.0.0.0" 1234 handler))
