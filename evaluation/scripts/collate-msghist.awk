/MSGHIST *[A-Z]+ / { count[$4] +=$5; bytes[$4] += $6 }
END { for (msg in count)
        printf("%-20s %-9d %-9d\n",msg,count[msg],bytes[msg]);
    }
