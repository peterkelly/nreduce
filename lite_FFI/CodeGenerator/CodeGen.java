import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.Reader;
import java.io.StreamTokenizer;

/* Generate binding code for ELC and foreign c functions */

public class CodeGen {

	//-------------------
	// Main method
	//-------------------
	public static void main(String[] args) {
	    // Check for correct number of args
	    if (args.length < 1 || args.length > 2)
	    {
	        printUsage();
	        return;
	    }
	    
	    // Check if the definition file exist
	    String def_file = args[0];
	    if(!fileExist(def_file)){
	    	System.out.println("File: " + def_file + " does not exist");
	    }
		
	}
	
	//---------------------------
	// Output the program usage
	//---------------------------
	static protected void printUsage()
	{
		System.out.println("Usage: java CodeGen [definition_file]");
	    System.out.println("  definition_file - define function prototypes, etc.");
	}
	
	static protected boolean fileExist(String fileName){
		return (new File(fileName)).exists();
	}
	  
	//-----------------------------------
	// Configure the Code generator
	// - based on the given definition
	//-----------------------------------
	static protected void configureGA(String configFile)
	{
	    try
	    {
	      Reader in = new BufferedReader(new FileReader(configFile));
	      
	      // Parse each line
	      StreamTokenizer tok = new StreamTokenizer(in);
	      tok.wordChars(0x20, 0x7E); 
	      tok.lowerCaseMode(true);
	      tok.commentChar('#');
	      tok.eolIsSignificant(true);
	      for (int ttype = tok.nextToken();
	           ttype != StreamTokenizer.TT_EOF;
	           ttype = tok.nextToken())
	      {
	        if (ttype == StreamTokenizer.TT_EOL)
	          continue;
	        
	        String rawLine = tok.sval;
	        
	        String line = rawLine.replaceAll("\\s", "");
	        String[] values = line.split("=");
	        switch (values.length)
	        {
	          case 0:
	            continue;
	          case 1:
	            throw new RuntimeException("Error parsing line (key without value):\n" + line);
	          case 2:
	            ga.setAttribute(values[0], values[1]);
	            break;
	          default:
	            throw new RuntimeException("Error parsing line (too many tokens):\n" + line);
	        }
	      }
	    }
	    catch (Exception e)
	    {
	        System.out.println(e);
	        throw new RuntimeException("Could not read config file: " + configFile);
	    }
	}
}
