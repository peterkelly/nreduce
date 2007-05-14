// HTML BibTeX index generator
// Peter Kelly <pmk@cs.adelaide.edu.au>
//
// This program relies on javabib, available at:
// http://www-plan.cs.colorado.edu/henkel/stuff/javabib/

package htmlbib;

import java.io.File;
import java.io.FileReader;

import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.transform.OutputKeys;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import bibtex.dom.BibtexEntry;
import bibtex.dom.BibtexFile;
import bibtex.dom.BibtexString;
import bibtex.parser.BibtexParser;

public class HTMLBib
{
    static Document outDoc = null;
    static Map<String,String> ltitles = null;

    public static void main(String[] args)
	throws Exception
    {
	if (args.length < 3) {
	    System.err.println("Usage: HTMLBib <bibfile> <categoryfile> <papersdir>\n");
	    System.exit(1);
	}

	String bibFilename = args[0];
	String categoryFilename = args[1];
	String papersDir = args[2];

	// Read BibTeX file
	BibtexFile bibtexFile = new BibtexFile();
	BibtexParser parser = new BibtexParser(false);

	parser.parse(bibtexFile,new FileReader(bibFilename));
	ltitles = getTitles(bibtexFile);

	// Read categories file
	DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
	DocumentBuilder builder = factory.newDocumentBuilder();
	Document categoryDoc = builder.parse(categoryFilename);

	Element categoryRoot = categoryDoc.getDocumentElement();

	// Create output DOM
	Document document = outDoc = builder.newDocument();
	Element html = document.createElementNS("http://www.w3.org/1999/xhtml","html"); 
	document.appendChild(html);

	Element head = document.createElementNS("http://www.w3.org/1999/xhtml","head"); 
	Element title = document.createElementNS("http://www.w3.org/1999/xhtml","title"); 
	Element body = document.createElementNS("http://www.w3.org/1999/xhtml","body"); 
	Element h1 = document.createElementNS("http://www.w3.org/1999/xhtml","h1"); 
	Element style = document.createElementNS("http://www.w3.org/1999/xhtml","style"); 

	String css = "body { font-family: helvetica }";

	// Create page template
	h1.setAttribute("style","text-align: center");
	h1.appendChild(document.createTextNode("Literature"));
	title.appendChild(document.createTextNode("Literature"));
	style.setAttribute("type","text/css");
	style.appendChild(document.createTextNode(css));
	head.appendChild(title);
	head.appendChild(style);
	body.appendChild(h1);
	html.appendChild(head);
	html.appendChild(body);

	// Build table of content
	genTOC(categoryRoot,1,body,"");

	// Process sections
	genSections(body,categoryRoot,1,ltitles,papersDir,"");


	// Write HTML output
	TransformerFactory tFactory = TransformerFactory.newInstance();
	Transformer transformer = tFactory.newTransformer();
	transformer.setOutputProperty(OutputKeys.INDENT,"yes");

	DOMSource source = new DOMSource(document);
	StreamResult result = new StreamResult(System.out);
	transformer.transform(source, result);
    }

    static void genTOC(Element parent, int depth, Element out, String pnum)
    {
	Element ol = null;
	int num = 1;

	NodeList children = parent.getChildNodes();
	int count = children.getLength();
	for (int i = 0; i < count; i++) {
	    Node child = children.item(i);
	    if ((child.getNodeType() == Node.ELEMENT_NODE) &&
		(child.getNodeName().equals("section"))) {
		String headName = "H"+(new Integer(depth)).toString();
		if (ol == null) {
		    ol = outDoc.createElementNS("http://www.w3.org/1999/xhtml","ol"); 
		    out.appendChild(ol);
		}
		Element li = outDoc.createElementNS("http://www.w3.org/1999/xhtml","li"); 
		Element a = outDoc.createElementNS("http://www.w3.org/1999/xhtml","a"); 
		a.setAttribute("href","#"+pnum+num);
		a.appendChild(outDoc.createTextNode(((Element)child).getAttribute("title")));
		li.appendChild(a);
		ol.appendChild(li);
		genTOC((Element)child,depth+1,li,pnum+num+".");
		num++;
	    }
	}

    }

    static void genSections(Element out, Element parent, int depth, Map<String,String> titles,
			    String papersdir, String pnum)
    {
	int num = 1;
	NodeList children = parent.getChildNodes();
	int count = children.getLength();

	Element p = outDoc.createElementNS("http://www.w3.org/1999/xhtml","p");
	out.appendChild(p);
	int done = 0;

	for (int i = 0; i < count; i++) {
	    Node child = children.item(i);
	    if ((child.getNodeType() == Node.ELEMENT_NODE) &&
		(child.getNodeName().equals("item"))) {
		Element childElem = (Element)child;
		String key = childElem.getAttribute("key");
		String filename = "";
		if ((new File(papersdir+"/"+key+".pdf")).exists())
		    filename = papersdir+"/"+key+".pdf";
		else if ((new File(papersdir+"/"+key+".ps.gz")).exists())
		    filename = papersdir+"/"+key+".ps.gz";
		else if ((new File(papersdir+"/"+key+".ps")).exists())
		    filename = papersdir+"/"+key+".ps";
		else if ((new File(papersdir+"/"+key+".html")).exists())
		    filename = papersdir+"/"+key+".html";

		if (filename.length() > 0) {
		    Element a = outDoc.createElementNS("http://www.w3.org/1999/xhtml","a");
		    a.setAttribute("href",filename);
		    a.appendChild(outDoc.createTextNode(titles.get(key)));
		    p.appendChild(a);
		    p.appendChild(outDoc.createTextNode(String.format(" [%s]",key)));
 		}
 		else {
		    p.appendChild(outDoc.createTextNode(String.format("%s [%s]",
								      titles.get(key),key)));
 		}
		Element br = outDoc.createElementNS("http://www.w3.org/1999/xhtml","br");
		p.appendChild(br);
	    }
	}

	for (int i = 0; i < count; i++) {
	    Node child = children.item(i);
	    if ((child.getNodeType() == Node.ELEMENT_NODE) &&
		(child.getNodeName().equals("section"))) {
		Element childElem = (Element)child;
		String headname = "h"+depth;
		Element a = outDoc.createElementNS("http://www.w3.org/1999/xhtml","a");
		a.setAttribute("name",pnum+num);
		out.appendChild(a);
		Element h = outDoc.createElementNS("http://www.w3.org/1999/xhtml",headname);
		h.appendChild(outDoc.createTextNode(String.format("%s%d %s",
								  pnum,num,
								  childElem.getAttribute("title"))));
		out.appendChild(h);
		genSections(out,childElem,depth+1,titles,papersdir,pnum+num+".");
		num++;
	    }
	}
    }

    static Map<String,String> getTitles(BibtexFile bibtexFile)
    {
	Map<String,String> titles = new HashMap<String,String>();

	List entries = bibtexFile.getEntries();
	Iterator it = entries.iterator();
	while (it.hasNext()) {
	    Object cur = it.next();
	    if (cur instanceof BibtexEntry) {
		BibtexEntry entry = (BibtexEntry)cur;
		Map fields = entry.getFields();
		String key = entry.getEntryKey();
		String title = ((BibtexString)fields.get("title")).getContent();
		title = title.replaceAll("(\\{|\\})","");
		title = title.replaceAll(" +"," ");
		titles.put(key,title);
	    }
	}

	return titles;
    }
}
