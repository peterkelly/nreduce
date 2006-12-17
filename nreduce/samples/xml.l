XML_DECL = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"

TYPE_ELEMENT = 0
TYPE_TEXT = 1
TYPE_ATTRIBUTE = 2

TOKEN_STARTELEM = 0
TOKEN_ENDELEM = 1
TOKEN_TEXT = 2
TOKEN_ATTRNAME = 3
TOKEN_ATTRVALUE = 4

TOKEN_NAMES = "startelem", "endelem", "text", "attrname", "attrvalue", nil

////////////////////////////////////////////////////////////////////////////////

print-token val =
(if val
    (if (= (head val) '\r')
        (append "\\r" (print-token (tail val)))
    (if (= (head val) '\n')
        (append "\\n" (print-token (tail val)))
    (cons (head val) (print-token (tail val)))))
    nil)

print-tokens !tokens =
(if (not tokens)
    nil
    (letrec
       !tok = (head tokens)
       !rest = (tail tokens)
       in
         (appendn 5 (item (head tok) TOKEN_NAMES) ": \""
                    (print-token (tail tok)) "\"\n"
                    (print-tokens rest))))

////////////////////////////////////////////////////////////////////////////////
isspace c = (or (= c ' ') (or (= c '\r') (= c '\n')))

parse_main !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '<')
        (if (> count 0)
            (cons (cons TOKEN_TEXT (prefix count start))
                  (parse_namestart rest))
            (parse_namestart rest))
        (parse_main rest start (+ count 1))))
    nil)

parse_pi !stream =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '?')
        (parse_piend rest)
        (parse_pi rest)))
    nil)

parse_piend !stream =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '>')
        (parse_skipspace rest)
        (parse_pi rest)))
    nil)

parse_skipspace !stream =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (isspace c)
        (parse_skipspace rest)
        (parse_main stream stream 0)))
    nil)

parse_namestart !stream =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '/')
        (parse_endname rest rest 0)
    (if (= c '?')
        (parse_pi rest)
        (parse_name stream stream 0))))
    nil)

parse_name !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '>')
        (cons (cons TOKEN_STARTELEM (prefix count start))
              (parse_main rest rest 0))
    (if (isspace c)
        (cons (cons TOKEN_STARTELEM (prefix count start))
              (parse_attrsearch rest))
        (parse_name rest start (+ count 1)))))
    nil)

parse_endname !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '>')
        (cons (cons TOKEN_ENDELEM nil)
              (parse_main rest rest 0))
        (parse_endname rest start (+ count 1))))
    nil)

parse_leafend !stream =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '>')
        (cons (cons TOKEN_ENDELEM nil)
              (parse_main rest rest 0))
        nil))
    nil)

parse_attrsearch !stream =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '>')
        (parse_main rest rest 0)
    (if (= c '/')
        (parse_leafend rest)
    (if (isspace c)
        (parse_attrsearch rest)
        (parse_attrname rest stream 0)))))
    nil)

parse_attrname !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (or (= c '=') (isspace c))
        (cons (cons TOKEN_ATTRNAME (prefix (+ count 1) start))
              (parse_eqsearch stream))
        (parse_attrname rest start (+ count 1))))
    nil)

parse_eqsearch !stream =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '=')
        (parse_valsearch rest)
    (if (isspace c)
        (parse_eqsearch rest)
        nil)))
    nil)

parse_valsearch !stream =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '>')
        (parse_main rest rest 0)
    (if (= c '\"')
        (parse_value rest rest 0)
    (if (isspace c)
        (parse_valsearch rest)
        nil))))
    nil)

parse_value !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '\"')
        (cons (cons TOKEN_ATTRVALUE (prefix count start))
              (parse_attrsearch rest))
        (parse_value rest start (+ count 1))))
    nil)

////////////////////////////////////////////////////////////////////////////////
last !lst =
(if (not lst)
    nil
(if (not (tail lst))
    (head lst)
    (last (tail lst))))

////////////////////////////////////////////////////////////////////////////////
allbutlast !lst =
(if (not lst)
    nil
(if (not (tail lst))
    nil
    (letrec
       !a = (head lst)
       !b = (tail lst)
     in
       (cons a (allbutlast b)))))

////////////////////////////////////////////////////////////////////////////////

mkelem name attributes children =
(cons TYPE_ELEMENT (cons name (cons attributes (cons children nil))))

mkattr name value =
(cons TYPE_ATTRIBUTE (cons name (cons value nil)))

getchildren lst =
(if lst
    (if (= (head (head lst)) TYPE_ATTRIBUTE)
        (getchildren (tail lst))
        lst)
    nil)

getattributes lst =
(if lst
    (if (= (head (head lst)) TYPE_ATTRIBUTE)
        (cons (head lst) (getattributes (tail lst)))
        nil)
    nil)

construct !tokens =
(if (not tokens)
    (cons nil nil)
(letrec
  !cur = (head tokens)
  !rest = (tail tokens)
  !tag = (head cur)
in
  (if (= tag TOKEN_STARTELEM)
      (letrec
         !pair = (construct rest)
         !contents = (allbutlast pair)
         !children = (getchildren contents)
         !attributes = (getattributes contents)
         !remainingtokens = (last pair)
         !name = (tail cur)
         !element = (mkelem name attributes children)
       in
         (cons element (construct remainingtokens)))
  (if (= tag TOKEN_ENDELEM)
      (cons rest nil)
  (if (= tag TOKEN_ATTRNAME)
      (letrec
         !value = (head rest)
         rest2 = (tail rest)
         attr = (mkattr (tail cur) (tail value))
       in
         (cons attr (construct rest2)))
  (if (= tag TOKEN_TEXT)
      (letrec
         !text = (tail cur)
       in
         (cons (cons TYPE_TEXT (cons text nil)) (construct rest)))
  (construct rest)))))))

construct1 !tokens = (allbutlast (construct tokens))

////////////////////////////////////////////////////////////////////////////////

apmap lst fun = (if lst (append (fun (head lst)) (apmap (tail lst) fun)) nil)

printnodes lst = (apmap lst printnode)

printnode node =
(if (= (head node) TYPE_ELEMENT)
    (if (item 3 node)
        (appendn 8 "<" (item 1 node) (printattrs (item 2 node)) ">"
                   (printnodes (item 3 node))
                   "</" (item 1 node) ">")
        (appendn 4 "<" (item 1 node) (printattrs (item 2 node)) "/>"))
(if (= (head node) TYPE_TEXT)
    (item 1 node)
(if (= (head node) TYPE_ATTRIBUTE)
    "ATTRIBUTE"
"UNKNOWN")))

printattrs lst = (apmap lst (!attr.appendn 5 " " (item 1 attr) "=\"" (item 2 attr) "\""))


////////////////////////////////////////////////////////////////////////////////

main =
(appendn 3 XML_DECL (printnodes (construct1 (parse_skipspace (readb "samples/test.xml")))) "\n")
