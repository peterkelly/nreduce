days = (listn 7 "Monday\n"
                "Tuesday\n"
                "Wednesday\n"
                "Thursday\n"
                "Friday\n"
                "Saturday\n"
                "Sunday\n")

show lst = (if lst (append (head lst) (show (tail lst))) nil)

main = (show days)
