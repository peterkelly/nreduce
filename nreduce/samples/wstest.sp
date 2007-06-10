import xml

WSDL_NS = "http://schemas.xmlsoap.org/wsdl/"

function getchildren2(lst,nsuri,localname)
{
  if (lst) {
    let cur = head(lst);
    if (xml::item_type(cur) == xml::TYPE_ELEMENT &&
        streq(xml::item_nsuri(cur),nsuri) &&
        streq(xml::item_localname(cur),localname))
      [cur];
    else
      nil;
    getchildren2(tail(lst),nsuri,localname);
  }
}

function getattr2(lst,nsuri,localname)
{
  if (lst) {
    let cur = head(lst);
    if (streq(xml::item_nsuri(cur),nsuri) &&
        streq(xml::item_localname(cur),localname))
      xml::item_value(cur);
    else
      getattr2(tail(lst),nsuri,localname);
  }
}

function getattr(elem,nsuri,localname)
{
  getattr2(xml::item_attributes(elem),nsuri,localname);
}

function lst_getattr(lst,nsuri,localname)
{
  apmap(lambda (elem) { getattr(elem,nsuri,localname); },lst);
}

function apmap(f,lst)
{
  if (lst) {
    f(head(lst));
    apmap(f,tail(lst));
  }
}

function getchildren(parents,nsuri,localname)
{
  apmap(lambda (parent) {
      getchildren2(xml::item_children(parent),nsuri,localname); },parents);
}

function getbyattrval(elements,nsuri,localname,value)
{
  if (elements) {
    let elem = head(elements);
    let thisval = getattr(elem,nsuri,localname);
    if (streq(thisval,value))
      elem;
    else
      getbyattrval(tail(elements),nsuri,localname,value);
  }
}

function operation_input(messages,operation)
{
  let inputs = getchildren([operation],WSDL_NS,"input");
  let inmsgname = getattr(inputs[0],"","name");
  getbyattrval(messages,"","name",inmsgname);
}

function operation_output(messages,operation)
{
  let outputs = getchildren([operation],WSDL_NS,"output");
  let outmsgname = getattr(outputs[0],"","name");
  getbyattrval(messages,"","name",outmsgname);
}

function process_wsdl(doc)
{
  let definitions = getchildren([doc],WSDL_NS,"definitions");
  let portTypes = getchildren(definitions,WSDL_NS,"portType");
  let services = getchildren(definitions,WSDL_NS,"service");
  let messages = getchildren(definitions,WSDL_NS,"message");

  foreach p (portTypes) {
    let operations = getchildren([p],WSDL_NS,"operation");
    getattr(p,"","name")." {\n\n";
    foreach o (operations) {
      let inmsg = operation_input(messages,o);
      let outmsg = operation_output(messages,o);
      let name = getattr(o,"","name");

      let inparts = getchildren([inmsg],WSDL_NS,"part");
      let outparts = getchildren([outmsg],WSDL_NS,"part");

      let ret_type = xml::getlocalname(getattr(outparts[0],"","type"));

      let show_parts = lambda (lst) {
        if (lst) {
          let p = head(lst);
          let type = xml::getlocalname(getattr(p,"","type"));
          let name = getattr(p,"","name");
          type." ".name;
          if (tail(lst))
            ", ";
          show_parts(tail(lst));
        }
      };

      "    ".ret_type." ".name."(";
      show_parts(inparts);
      ");\n\n";

    }
    "}\n";
  }
}

function main(args)
{
  if (len(args) == 0)
    "Usage: wstest <filename>\n";
  else {
    let filename = args[0];

    let content = readb(filename);
    let doc = xml::parsexml(1,content);
    process_wsdl(doc);
  }
}
