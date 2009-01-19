declare function local:nfib($n)
{
  if ($n le 2) then
    1
  else
    local:nfib($n - 2) + local:nfib($n - 1)
};

local:nfib(32)
