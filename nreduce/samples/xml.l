STATE_MAIN = 0
STATE_NAMESTART = 1
STATE_NAME = 2
STATE_ENDNAME = 3
STATE_ATTRSEARCH = 4
STATE_ATTRNAME = 5

TOKEN_STARTELEM = 0
TOKEN_ENDELEM = 1
TOKEN_TEXT = 2

TOKEN_NAMES = "startelem", "endelem", "text", nil

TYPE_ELEMENT = 0
TYPE_TEXT = 1

////////////////////////////////////////////////////////////////////////////////
print-tokens !tokens =
{
  if (not tokens) nil
  (letrec
     !tok = (head tokens)
     !rest = (tail tokens)
   in
     (append (item (head tok) TOKEN_NAMES)
     (append ": \""
     (append (tail tok)
     (append "\"\n"
     (print-tokens rest))))))
}

////////////////////////////////////////////////////////////////////////////////
parse1 !rest !start !count = (parse_namestart rest start (+ count 1))

parse_main !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '<')
        (cons (cons TOKEN_TEXT (prefix count start))
              (parse1 rest start count))
              (parse_main rest start (+ count 1))))
    nil)

parse_namestart !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '/')
        (parse_endname rest rest 0)
        (parse_name stream stream 0)))
    nil)

parse_name !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '>')
        (cons (cons TOKEN_STARTELEM (prefix count start))
              (parse_main rest rest 0))
        (parse_name rest start (+ count 1))))
    nil)

parse_endname !stream !start !count =
(if stream
    (letrec !c = (head stream) !rest = (tail stream) in
    (if (= c '>')
        (cons (cons TOKEN_ENDELEM (prefix count start))
              (parse_main rest rest 0))
        (parse_endname rest start (+ count 1))))
    nil)

////////////////////////////////////////////////////////////////////////////////
last !lst =
{
  if (not lst) {
    nil
  } { if (not (tail lst)) {
    (head lst)
  } {
    (last (tail lst))
  }}
}

////////////////////////////////////////////////////////////////////////////////
allbutlast !lst =
{
  if (not lst) {
    nil
  } { if (not (tail lst)) {
    nil
  } {
    letrec
      !a = (head lst)
      !b = (tail lst)
    in
      (cons a (allbutlast b))
  }}
}

////////////////////////////////////////////////////////////////////////////////
construct !tokens =
{
  if (not tokens) {
    (cons nil nil)
  } {
    letrec
      !cur = (head tokens)
      !rest = (tail tokens)
      !tag = (head cur)
    in {
      if (= tag TOKEN_STARTELEM) {
        letrec
          !r = (construct rest)
          !children = (allbutlast r)
          !rest2 = (last r)
        in {
          letrec
            !a = (tail cur)
          in
            (cons (cons TYPE_ELEMENT (cons a (cons children nil))) (construct rest2))
        }
      } {
      if (= tag TOKEN_ENDELEM) {
        (cons rest nil)
      } {
      if (= tag TOKEN_TEXT) {
        (letrec !text = (tail cur) in
        (cons (cons TYPE_TEXT (cons text nil)) (construct rest)))
      } {
        (construct rest)
      }}}
    }
  }
}

construct1 !tokens = (allbutlast (construct tokens))

////////////////////////////////////////////////////////////////////////////////
printindent !indent =
{
//  if (> indent 0)
//    (append "  " (printindent (- indent 1)))
//    nil
  nil
}

////////////////////////////////////////////////////////////////////////////////
printxml !indent !tree !next =
{
  if (not tree)
    next
  (letrec
     !cur = (head tree)
     !rest = (tail tree)
     !tag = (head cur)
   in
     (if (= tag TYPE_ELEMENT)
         (letrec !name = (head (tail cur))
                 !nextindent = (+ indent 1)
                 !children = (head (tail (tail cur)))
          in
           (append "<"
           (append name
           (append ":"
           (append ">"
           (printxml nextindent children
           (append "</"
           (append name
           (append ">" (printxml indent rest next))))))))))
     (if (= tag TYPE_TEXT)
         (letrec !a = (head (tail cur)) in
         (append a (printxml indent rest next)))
         (printxml indent rest next))))
}

////////////////////////////////////////////////////////////////////////////////
main =
{
  letrec
    stream = (readb "samples/test.xml")
  in
//    (print-tokens (parse stream STATE_MAIN stream 0))
    (printxml 0 (construct1 (parse_main stream stream 0)) nil)
}
