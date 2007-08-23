import http

function httpheader()
{
  "HTTP/1.1 200 OK\r\n";
  "Content-Type: text/plain; charset=UTF-8\r\n";
  "Connection: close\r\n";
  "\r\n";
}

function print_dirs(lst)
{
  if (lst) {
    let entry = head(lst);
    let rest = tail(lst);
    if (streq(entry[1],"directory"))
      "<li><a href=\"".entry[0]."/\">".entry[0]."</a></li>\n";
    print_dirs(rest);
  }
}

function print_files(lst)
{
  if (lst) {
    let entry = head(lst);
    let rest = tail(lst);
    if (streq(entry[1],"file")) {
      let name = entry[0];
      let size = entry[2];
      "<li><a href=\"".name."\">".name."</a> ".numtostring(size)."</li>\n";
    }
    print_files(rest);
  }
}

function handle_dir(uri)
{
  let contents = readdir(uri);
  "HTTP/1.1 200 OK\r\n";
  "Content-Type: text/html; charset=UTF-8\r\n";
  "Connection: close\r\n";
  "\r\n";
  "<html><body><h1>Contents of ".uri."</h1>Directories<ul>";
  print_dirs(contents);
  "</ul>Files<ul>";
  print_files(contents);
  "</ul>End</body>";
}

function process(method,uri,version,headers,body)
{
  if (not(exists(uri)))
    httpheader."File ".uri." does not exist";
  else if (isdir(uri))
    handle_dir(uri);
  else
    httpheader.readb(uri);
}

function handler(stream)
{
  if (stream)
  http::parse_request(stream,process);
  else
    nil;
}

function main()
{
  parlist(listen(1234,handler));
}
