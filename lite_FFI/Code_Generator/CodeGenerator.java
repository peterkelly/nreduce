import java.io.File;

/* Generate binding code for ELC and foreign c functions */

public class CodeGenerator {

	//-------------------
	// Main method
	//-------------------
	public static void main(String[] args) {
	    String def_dir = "DefinitionFile/";
	    // Check for correct number of args
	    if (args.length < 1 || args.length > 2)
	    {
	        printUsage();
	        return;
	    }
	    
	    // Check if the definition file exist
	    String def_file = def_dir + args[0];
	    if(!fileExist(def_file)){
	    	System.out.println("File: " + def_file + " does not exist");
	    } else {
	        // Process the file
	        XSLTProcessor defProcessor = new XSLTProcessor(def_file);
	        defProcessor.genELCWrappers();
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
	  

}
