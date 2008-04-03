import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;

/**
 *  Used to parse definition file
 **/

public class DefinitionReader {
	private String fileName;	//The definition file
	private BufferedReader fileReader;
	private ArrayList<String> allFunctions = new ArrayList<String>();	//Store all the contents of the definition file
	
	public DefinitionReader(String fileName){
		this.fileName = fileName;
		try{
			fileReader = new BufferedReader(new FileReader(fileName));
		}catch(IOException ioe){
			System.out.println("Could not find the file: " + fileName);
		}
		
		parseDefinitionFile(fileReader);
	}

	/**
	 * Parse the definition file, and store its contents line by line onto an Arraylist
	 * @param fileReader
	 */
	private void parseDefinitionFile(BufferedReader fileReader){
		String lineData;
		String[] functionData = null;
		
		try{
			lineData = fileReader.readLine();
			while(lineData != null){
				 functionData = lineData.split(" ");
				 
				 //Check the vadility of function data
				 if(functionData.length <= 1){
					 lineData = fileReader.readLine();
					 continue;
				 }
				 
				 //Parse the function prototype
				 
			}
		}catch(IOException ioe){
			System.out.println("Error in reading file");
		}
	}
	
	
	/** 
	 * Return a function info, including its 
	 * name; return type; Abbreviation; parameter types and the corresponding abbreviations
	 * (refer to the relevant documents to see the abbreviations)
	 */ 
	public ArrayList<String> nextFunction(){
		
	}
	
	
}
