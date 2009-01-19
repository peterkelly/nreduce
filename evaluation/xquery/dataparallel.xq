import service namespace comp = "http://__HOSTNAME__:__PORT__/compute?WSDL";

<result>
{ for $i in 1 to 1024
  return comp:compute($i,1000) }
</result>
