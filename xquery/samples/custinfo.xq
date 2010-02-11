(:

Illustrates a simple database lookup. The workflow first retrieves a list of
customer ids, and then for each, looks up the customer info associated with
that id. All of the customerInfo() calls can run in parallel.

:)


<customers>
{for $i in svc:customerIds()/*
 return svc:customerInfo($i);
}</customers>
