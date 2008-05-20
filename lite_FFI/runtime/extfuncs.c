#include "extfuncs.h"

const extfunc extfunc_info[NUM_EXTFUNCS];

int get_extfunc(const char *name)
{
  int i;
  for (i = 0; i < NUM_EXTFUNCS; i++)
    if (!strcmp(name,extfunc_info[i].name))
      return i;
  return -1;
}

/******** util functions *********/
//// similar to b_cons(), which returns a pntr pointing to a cell
pntr make_cons(task *tsk, pntr head, pntr tail)
{
	pntr newCellPntr;
	cell *res = alloc_cell(tsk);
	res->type = CELL_CONS;
	res->field1 = head;
	res->field2 = tail;

	make_pntr(newCellPntr,res);
	return newCellPntr;
}


pntr head(task *tsk, pntr pCell){
    pntr cellHead;
    if(pntrtype(pCell) == CELL_CONS){
        cellHead = get_pntr(pCell)->field1;
    }else {
        set_error(tsk,"head: expected cons, got %s",cell_types[pntrtype(pCell)]);
    }
    
    return resolve_pntr(cellHead);
}

pntr tail(task *tsk, pntr pCell){
    pntr cellTail;
    
    if(pntrtype(pCell) == CELL_CONS){
        cellTail = get_pntr(pCell)->field2;
    }else {
        set_error(tsk,"tail: expected cons, got %s",cell_types[pntrtype(pCell)]);
    }
    
    return resolve_pntr(cellTail);
}
//// get the last cell from a list
cell* get_last_cell(task *tsk, pntr *list)
{
	pntr tail;
	cell *lastCell;
	
	if( pntrtype(*list) != CELL_CONS ){
		return NULL;
	}else{
		lastCell = get_pntr(*list);
		tail = resolve_pntr(get_pntr(*list)->field2);
		while(!pntrequal(tail, tsk->globnilpntr)){
			lastCell = get_pntr(tail);
			tail = resolve_pntr(lastCell->field2);
		}
		return lastCell;
	}
}

//// concatenate 2 lists, which are formed with (cons * (cons * ... nil))
pntr connect_lists(task *tsk, pntr *list1, pntr *list2)
{
	cell *lastCell = get_last_cell(tsk, list1);
	
	make_pntr(lastCell->field2, get_pntr(*list2));
	return *list1;
}


//// increase the memory space allocated to the string (str)
char* string_mkroom(char *str, const int newSize)
{
	return realloc(str, newSize);
}

//// my version of data_to_list
/*
pntr data_to_list(task *tsk, const char *data, int size, pntr tail)
{
	int i;
	pntr preList;
	
	if(size <= 0){
		return tail;
	}
	
	cell *lastCell = alloc_cell(tsk);
	lastCell->type = CELL_CONS;
	set_pntrdouble(lastCell->field1, data[size-1]);
	lastCell->field2 = tsk->globnilpntr;
	make_pntr(preList, lastCell);
	
	for(i=size-2; i>=0; i--){
		cell *ch = alloc_cell(tsk);
    	ch->type = CELL_CONS;
    	set_pntrdouble(ch->field1,data[i]);
    	ch->field2 = preList;
    	make_pntr(preList, ch);
	}
	
	return preList;
}
*/


/****************** external functions ************************/

//// read a file contents from either a archive of real directory
void b_zzip_read(task *tsk, pntr *argstack)
{
	char *fileName;
	pntr p = argstack[0];
	int badtype;

	CHECK_ARG(0, CELL_CONS);
	if((badtype = array_to_string(p, &fileName)) >= 0){
		set_error(tsk, "error1: argument is not a string (contains non-char: %s)", cell_types[badtype]);
		return;
	}
	
	ZZIP_FILE* fp = zzip_open (fileName, O_RDONLY|O_BINARY);

    if (! fp){
   		perror (fileName);
    }
    
    int bufSize = 2, blockSize = 1024, numBlocks = 1;
    char buf[bufSize];
    int n, counter = 0;
    char *contents = (char *)calloc(blockSize, sizeof(char));
    
    /* read chunks of bufSize bytes into buf and concatenate them into previous string */
    while( (n = (zzip_read(fp, buf, bufSize-1))) > 0){
    	counter++;
    	
    	if(counter == 1){
//    		strcpy(contents, buf);
			strncat(contents, buf, bufSize-1);
			bufSize = 21;
    	}else{
    		int originalSize = strlen(contents);
    		if( ((blockSize*numBlocks) - (originalSize + 1)) < bufSize){
    			numBlocks++;
    			contents = string_mkroom(contents, blockSize*numBlocks);
    		}

    		strncat(contents, buf, bufSize-1);
//    		printf("%s\n\n\n", contents);
    	}
    	buf[n] = '\0';
    }
    
    argstack[0] = string_to_array(tsk, contents);
	
	zzip_close(fp);
}

//// Check if the given file is a real directory or a Zip-archive
static void b_zzip_dir_real(task *tsk, pntr *argstack)
{
	char *fileName;
	int badtype;
	pntr p = argstack[0];
	int dirtype;
	
	CHECK_ARG(0, CELL_CONS);
	if((badtype = array_to_string(p, &fileName)) >= 0){
		set_error(tsk, "error1: argument is not a string (contains non-char: %s)", cell_types[badtype]);
		return;
	}
	
	//// the file to be ckecked exists, then we check it
	static const char* ext[] = { "", ".exe", ".EXE", 0 };
	ZZIP_DIR* dir = zzip_opendir_ext_io (fileName, ZZIP_PREFERZIP, ext, 0);

    if(dir){
    	if(zzip_dir_real(dir)){
    		dirtype = 1;
//	   		printf("%s is a directory.\n", fileName);
    	}else{
    		dirtype = 0;
//    		printf("%s is a zip-archive.\n", fileName);
    	}
//    	zzip_dir_close(dir);
    }else{
    	//// file failed to be open
    	dirtype = -1;
    }
	setnumber(&argstack[0], dirtype);
	
	return;
}

static void b_zzip_version(task *tsk, pntr *argstack)
{
//	char *version;
}

//// read the directory entries of given dir/archive
static void b_zzip_read_dirent(task *tsk, pntr *argstack)
{
	char *fileName;
	pntr p = argstack[0];
	int badtype;
	
	CHECK_ARG(0, CELL_CONS);
	if((badtype = array_to_string(p, &fileName)) >= 0){
		set_error(tsk, "error1: argument is not a string (contains non-char: %s)", cell_types[badtype]);
		return;
	}

    ZZIP_DIR * dir;
    ZZIP_DIRENT * d;
  
    dir = zzip_opendir(fileName);
    if (! dir){
    	fprintf (stderr, "did not open %s: ", fileName);
    	set_error(tsk, "error1: could not handle file: %s", fileName);
		return;
    }

    char *singleFileName;
    char *compressionType;
    int fSize = 20;
    char fileSize[fSize];
    char compressedSize[fSize];
    pntr pSingleFileName, pCompressionType, pFileSize, pCompressedSize;
    pntr preList, singleList;
    int counter = 0;
    
	/* read each dir entry, a list for each file */
	while ((d = zzip_readdir (dir))){
		counter++;
		/* orignal size / compression-type / compression-ratio / filename */
		singleFileName = d->d_name;
		pSingleFileName = string_to_array(tsk, singleFileName);	//// convert the string to cons list
		
//		sprintf(compressionType, "%s ", zzip_compr_str(d->d_compr)); //// NOTE: executing this func will change the tsk->steamstack, very weird
																	//// NOTE: overflow caused here
		compressionType = (char *)zzip_compr_str(d->d_compr);
		pCompressionType = string_to_array(tsk, compressionType);
		
//		snprintf(fileSize, 5, "%d ", d->st_size);	//// NOTE: executing this func will change the tsk->steamstack, very weird
		format_double(fileSize, fSize, d->st_size);
		pFileSize = string_to_array(tsk, fileSize);

//		sprintf(compressedSize, "%d ", d->d_csize);
		format_double(compressedSize, fSize, d->d_csize);
		pCompressedSize = string_to_array(tsk, compressedSize);
		
//		printf("cell type: %s \t", cell_type(preList));
		//// link the cons lists to form a new list
//		singleList = connect_lists(tsk, &pSingleFileName, &pCompressionType);
//		singleList = connect_lists(tsk, &singleList, &pFileSize);
//		singleList = connect_lists(tsk, &singleList, &pCompressedSize);
		
		//// make cons from the last element to the beginning element
		singleList = make_cons(tsk, pCompressedSize, tsk->globnilpntr);
		singleList = make_cons(tsk, pFileSize, singleList);
		singleList = make_cons(tsk, pCompressionType, singleList);
		singleList = make_cons(tsk, pSingleFileName, singleList);
		
		
		if(counter == 1){
			preList = make_cons(tsk, singleList, tsk->globnilpntr);
//			printf("cell type: %s \t", cell_type(preList));
		}else{
			preList = make_cons(tsk, singleList, preList);
//			printf("cell type: %s \n", cell_type(preList));
		}
	}

	argstack[0] = preList;
//	printf("cell type: %s \n", cell_type(argstack[0]));
}

/*********************** Test drawRectangles1 ***************************************/


/* Testing:            *********enlargeRectangle1*************  */
typedef struct Rectangle Rectangle;
typedef struct Color Color;
typedef struct gray gray;

struct Rectangle {
    int width;
    char *creator;
    int height;
    Color *col;
};

struct Color {
    int red;
    int blue;
    int green;
    gray *cg;
};

struct gray {
    char *country;
    int grayCode;
};



/* Declear the struct constructor and destructor */
Rectangle *new_Rectangle();
//Rectangle *new_Rectangle(char *userName);
void free_Rectangle(Rectangle *f_Rectangle);
Color *new_Color();
void free_Color(Color *f_Color);
gray *new_gray();
void free_gray(gray *f_gray);

/*
Rectangle *new_Rectangle(char *userName){
    Rectangle *r= new_Rectangle();
    r->creator = userName;
    
    return r;
}
*/

/* Definition of the constructors and destructors */
Rectangle *new_Rectangle() {
    Rectangle *ret = malloc(sizeof(Rectangle));
    return ret;
}
void free_Rectangle(Rectangle *f_Rectangle) {
    free_Color(f_Rectangle->col);
    free(f_Rectangle);
}

Color *new_Color() {
    Color *ret = malloc(sizeof(Color));
    return ret;
}
void free_Color(Color *f_Color) {
    free_gray(f_Color->cg);
    free(f_Color);
}

gray *new_gray() {
    gray *ret = malloc(sizeof(gray));
    return ret;
}
void free_gray(gray *f_gray) {
    free(f_gray);
}

int drawRectangles(char *userName, double xPos, int yPos, Rectangle *rect){
    //Check the arguments

    if(strcmp(userName, "SmAxlL")){
        return 0;
    }
    
    if(xPos != 1.5){
        return 0;
    }
    
    if(yPos != 2){
        return 0;
    }
    
    if( (rect->width != 10) |  (rect->height != 15) | (rect->col->red != 100) | (rect->col->blue != 150) | (rect->col->green != 200) ){
        return 0;
    }
    
    
    
    return 1;
}


static void b_drawRectangles1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[3]; // userName
    pntr val2 = argstack[2]; // xPos
    pntr val3 = argstack[1]; // yPos
    pntr val4 = argstack[0]; // rect
    int badtype;

    /* the result value to be return */
    int numRect;

    char *userName;
    double xPos;
    int yPos;
    Rectangle *rect = new_Rectangle();

    /* Check validity of each parameter */
    CHECK_ARG(3, CELL_CONS);
    CHECK_ARG(2, CELL_NUMBER);
    CHECK_ARG(1, CELL_NUMBER);
    CHECK_ARG(0, CELL_CONS);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &userName)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    xPos = pntrdouble(val2);
    yPos = pntrdouble(val3);

    /* Initialize the struct: Rectangle */
    pntr rect_val1 = head(tsk, val4);
    rect->width = pntrdouble(rect_val1);
    pntr rect_val2 = head(tsk, tail(tsk, val4));
    if( (badtype = array_to_string(rect_val2, &(rect->creator) )) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    pntr rect_val3 = head(tsk, tail(tsk, tail(tsk, val4)));
    rect->height = pntrdouble(rect_val3);

    /* Initialize another struct: Color*/
    Color *rect_col = new_Color( );
    rect->col = rect_col;
    /* new root pntr for struct Color */
    pntr rect_col_val = head(tsk, tail(tsk, tail(tsk, tail(tsk, val4))));
    pntr rect_col_val1 = head(tsk, rect_col_val);
    rect_col->red = pntrdouble(rect_col_val1);
    pntr rect_col_val2 = head(tsk, tail(tsk, rect_col_val));
    rect_col->blue = pntrdouble(rect_col_val2);
    pntr rect_col_val3 = head(tsk, tail(tsk, tail(tsk, rect_col_val)));
    rect_col->green = pntrdouble(rect_col_val3);

    /* Initialize another struct: gray*/
    gray *rect_col_cg = new_gray( );
    rect_col->cg = rect_col_cg;
    /* new root pntr for struct gray */
    pntr rect_col_cg_val = head(tsk, tail(tsk, tail(tsk, tail(tsk, rect_col_val))));
    pntr rect_col_cg_val1 = head(tsk, rect_col_cg_val);
    if( (badtype = array_to_string(rect_col_cg_val1, &(rect_col_cg->country) )) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    pntr rect_col_cg_val2 = head(tsk, tail(tsk, rect_col_cg_val));
    rect_col_cg->grayCode = pntrdouble(rect_col_cg_val2);

    /* end Initialization of struct Rectangle */

    /* Call the method and get the return value */
    numRect = drawRectangles(userName, xPos, yPos, rect);
    
    setnumber(&argstack[0], numRect);
    
}

Rectangle *enlargeRect(Rectangle *oriRect, int times){
    oriRect->height = oriRect->height * times;
    oriRect->width = oriRect->width * times;
    oriRect->creator = "HUA";
    oriRect->col->blue = 222;
    oriRect->col->green = 333;
    oriRect->col->red = 444;
    oriRect->col->cg->country = "Australia";
    oriRect->col->cg->grayCode = 13;
    return oriRect;
}


/*    enlarge() */
static void b_enlargeRect1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[1]; // originalRect
    pntr val2 = argstack[0]; // times
    int badtype;

    /* the result value to be return */
    Rectangle *rect_return = new_Rectangle();

    Rectangle *originalRect = new_Rectangle();
    int times;

    /* Check validity of each parameter */
    CHECK_ARG(1, CELL_CONS);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */

    /* Initialize the struct: Rectangle */
    pntr originalRect_val1 = head(tsk, val1);
    originalRect->width = pntrdouble(originalRect_val1);
    pntr originalRect_val2 = head(tsk, tail(tsk, val1));
    if( (badtype = array_to_string(originalRect_val2, &(originalRect->creator) )) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    pntr originalRect_val3 = head(tsk, tail(tsk, tail(tsk, val1)));
    originalRect->height = pntrdouble(originalRect_val3);

    /* Initialize another struct: Color*/
    Color *originalRect_col = new_Color( );
    originalRect->col = originalRect_col;
    /* new root pntr for struct Color */
    pntr originalRect_col_val = head(tsk, tail(tsk, tail(tsk, tail(tsk, val1))));
    pntr originalRect_col_val1 = head(tsk, originalRect_col_val);
    originalRect_col->red = pntrdouble(originalRect_col_val1);
    pntr originalRect_col_val2 = head(tsk, tail(tsk, originalRect_col_val));
    originalRect_col->blue = pntrdouble(originalRect_col_val2);
    pntr originalRect_col_val3 = head(tsk, tail(tsk, tail(tsk, originalRect_col_val)));
    originalRect_col->green = pntrdouble(originalRect_col_val3);

    /* Initialize another struct: gray*/
    gray *originalRect_col_cg = new_gray( );
    originalRect_col->cg = originalRect_col_cg;
    /* new root pntr for struct gray */
    pntr originalRect_col_cg_val = head(tsk, tail(tsk, tail(tsk, tail(tsk, originalRect_col_val))));
    pntr originalRect_col_cg_val1 = head(tsk, originalRect_col_cg_val);
    if( (badtype = array_to_string(originalRect_col_cg_val1, &(originalRect_col_cg->country) )) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    pntr originalRect_col_cg_val2 = head(tsk, tail(tsk, originalRect_col_cg_val));
    originalRect_col_cg->grayCode = pntrdouble(originalRect_col_cg_val2);

    /* end Initialization of struct Rectangle */
    times = pntrdouble(val2);
    
    /* Call the method and get the return value */
    rect_return = enlargeRect(originalRect, times);
    
    /* Translate the resultant pntr to be return */

    /* Translate C struct to ELC struct */

    /* pntr for struct gray*/
    pntr p_rect_return_col_cg_country = string_to_array(tsk, rect_return->col->cg->country);
    pntr p_rect_return_col_cg_grayCode;
    set_pntrdouble(p_rect_return_col_cg_grayCode, rect_return->col->cg->grayCode);
    /* the root pntr for struct gray */
    pntr p_rect_return_col_cg = make_cons(tsk, p_rect_return_col_cg_country, make_cons(tsk, p_rect_return_col_cg_grayCode, tsk->globnilpntr));


    /* pntr for struct Color*/
    pntr p_rect_return_col_red;
    set_pntrdouble(p_rect_return_col_red, rect_return->col->red);
    pntr p_rect_return_col_blue;
    set_pntrdouble(p_rect_return_col_blue, rect_return->col->blue);
    pntr p_rect_return_col_green;
    set_pntrdouble(p_rect_return_col_green, rect_return->col->green);
    /* the root pntr for struct Color */
    pntr p_rect_return_col = make_cons(tsk, p_rect_return_col_red, make_cons(tsk, p_rect_return_col_blue, make_cons(tsk, p_rect_return_col_green, make_cons(tsk, p_rect_return_col_cg, tsk->globnilpntr))));


    /* pntr for struct Rectangle*/
    pntr p_rect_return_width;
    set_pntrdouble(p_rect_return_width, rect_return->width);
    pntr p_rect_return_creator = string_to_array(tsk, rect_return->creator);
    pntr p_rect_return_height;
    set_pntrdouble(p_rect_return_height, rect_return->height);
    /* the root pntr for struct Rectangle */
    pntr p_rect_return = make_cons(tsk, p_rect_return_width, make_cons(tsk, p_rect_return_creator, make_cons(tsk, p_rect_return_height, make_cons(tsk, p_rect_return_col, tsk->globnilpntr))));

    /* set the return value */
    argstack[0] = p_rect_return;

    /* Free the return struct */
    free_Rectangle(rect_return);
}

#define ThrowWandException(wand) \
{ \
  char \
    *description; \
 \
  ExceptionType \
    severity; \
 \
  description=MagickGetException(wand,&severity); \
  (void) fprintf(stderr,"%s %s %lu %s\n",GetMagickModule(),description); \
  description=(char *) MagickRelinquishMemory(description); \
  exit(-1); \
}

//int magickResizeImage(const char *imageFile, const char *outputImage, int columns, int rows, int magickFilter, double blur);

int magickResizeImage(const char *imageFile, const char *outputImage, int columns, int rows, int magickFilter, double blur){
    MagickBooleanType status;

    MagickWand *magick_wand;

    /*
      Read an image.
    */
    MagickWandGenesis();
    magick_wand=NewMagickWand();  
    status=MagickReadImage(magick_wand, (char *)imageFile);
    if (status == MagickFalse){
      ThrowWandException(magick_wand);
    }
    /*
      Turn the images into a thumbnail sequence.
    */
    MagickResetIterator(magick_wand);
    while (MagickNextImage(magick_wand) != MagickFalse)
      MagickResizeImage(magick_wand, columns, rows, LanczosFilter, blur);
    /*
      Write the image then destroy it.
    */
    status=MagickWriteImages(magick_wand, (char *)outputImage, MagickTrue);
    if (status == MagickFalse)
      ThrowWandException(magick_wand);
    magick_wand=DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    return status;
    
}



static void b_magickResizeImage1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[5]; // imageFile
    pntr val2 = argstack[4]; // outputImage
    pntr val3 = argstack[3]; // columns
    pntr val4 = argstack[2]; // rows
    pntr val5 = argstack[1]; // magickFilter
    pntr val6 = argstack[0]; // blur
    int badtype;

    /* the result value to be return */
    int result;

    char *imageFile;
    char *outputImage;
    int columns;
    int rows;
    int magickFilter;
    double blur;

    /* Check validity of each parameter */
    CHECK_ARG(5, CELL_CONS);
    CHECK_ARG(4, CELL_CONS);
    CHECK_ARG(3, CELL_NUMBER);
    CHECK_ARG(2, CELL_NUMBER);
    CHECK_ARG(1, CELL_NUMBER);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &imageFile)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val2, &outputImage)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    columns = pntrdouble(val3);
    rows = pntrdouble(val4);
    magickFilter = pntrdouble(val5);
    blur = pntrdouble(val6);

    /* Call the method and get the return value */
    result = magickResizeImage(imageFile, outputImage, columns, rows, magickFilter, blur);

    /* Translate the resultant pntr to be return */
    pntr p_result;
    set_pntrdouble(p_result, result);

    /* set the return value */
    argstack[0] = p_result;
}

int magickRotateImage(const char *imageFile, const char *outputImage, double degree){
    MagickBooleanType status;
    MagickWand *magick_wand;

    PixelWand * bg = malloc(sizeof(PixelWand *));
    /*
      Read an image.
    */
    MagickWandGenesis();
    magick_wand=NewMagickWand();  
    status=MagickReadImage(magick_wand, (char *)imageFile);
    if (status == MagickFalse){
      ThrowWandException(magick_wand);
    }
    /*
      Turn the images into a thumbnail sequence.
    */
    MagickResetIterator(magick_wand);
    while (MagickNextImage(magick_wand) != MagickFalse)
      MagickRotateImage(magick_wand, bg, degree);
//    MagickSetImageCompression(magick_wand, MW_JPEGCompression);
//    MagickUnsharpMaskImage( magick_wand, 4.5, 4.0, 4.5, 0.02 );
    /*
      Write the image then destroy it.
    */
    status=MagickWriteImages(magick_wand, (char *)outputImage, MagickTrue);
    if (status == MagickFalse)
      ThrowWandException(magick_wand);
    magick_wand=DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    return status;
}

static void b_magickRotateImage1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[2]; // imageFile
    pntr val2 = argstack[1]; // outputImage
    pntr val3 = argstack[0]; // degree
    int badtype;

    /* the result value to be return */
    int result;

    char *imageFile;
    char *outputImage;
    double degree;

    /* Check validity of each parameter */
    CHECK_ARG(2, CELL_CONS);
    CHECK_ARG(1, CELL_CONS);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &imageFile)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val2, &outputImage)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    degree = pntrdouble(val3);

    /* Call the method and get the return value */
    result = magickRotateImage(imageFile, outputImage, degree);

    /* Translate the resultant pntr to be return */
    pntr p_result;
    set_pntrdouble(p_result, result);

    /* set the return value */
    argstack[0] = p_result;
}

int magickChopImage(const char *imageFile, const char *outputImage, int width, int height, int xPos, int yPos){
    MagickBooleanType status;

    MagickWand *magick_wand;

    /*
      Read an image.
    */
    MagickWandGenesis();
    magick_wand=NewMagickWand();  
    status=MagickReadImage(magick_wand, (char *)imageFile);
    if (status == MagickFalse){
      ThrowWandException(magick_wand);
    }
    /*
      Turn the images into a thumbnail sequence.
    */
    MagickResetIterator(magick_wand);
    while (MagickNextImage(magick_wand) != MagickFalse)
      MagickChopImage(magick_wand, width, height, xPos, yPos);
    /*
      Write the image then destroy it.
    */
    status=MagickWriteImages(magick_wand, (char *)outputImage, MagickTrue);
    if (status == MagickFalse)
      ThrowWandException(magick_wand);
    magick_wand=DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    return status;
}

static void b_magickChopImage1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[5]; // imageFile
    pntr val2 = argstack[4]; // outputImage
    pntr val3 = argstack[3]; // width
    pntr val4 = argstack[2]; // height
    pntr val5 = argstack[1]; // xPos
    pntr val6 = argstack[0]; // yPos
    int badtype;

    /* the result value to be return */
    int result;

    char *imageFile;
    char *outputImage;
    int width;
    int height;
    int xPos;
    int yPos;

    /* Check validity of each parameter */
    CHECK_ARG(5, CELL_CONS);
    CHECK_ARG(4, CELL_CONS);
    CHECK_ARG(3, CELL_NUMBER);
    CHECK_ARG(2, CELL_NUMBER);
    CHECK_ARG(1, CELL_NUMBER);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &imageFile)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val2, &outputImage)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    width = pntrdouble(val3);
    height = pntrdouble(val4);
    xPos = pntrdouble(val5);
    yPos = pntrdouble(val6);

    /* Call the method and get the return value */
    result = magickChopImage(imageFile, outputImage, width, height, xPos, yPos);

    /* Translate the resultant pntr to be return */
    pntr p_result;
    set_pntrdouble(p_result, result);

    /* set the return value */
    argstack[0] = p_result;
}

//int magickCompressImage(char *imageFile, char *outputImage, char *format, int compressionType, double compressionRate);

int magickCompressImage(const char *imageFile, const char *outputImage, const char *format, int compressionType, double compressionRate){
    MagickBooleanType status;

    MagickWand *magick_wand;

    /*
      Read an image.
    */
 //   printf("compressionRate: %d", compressionRate);
    MagickWandGenesis();
    magick_wand=NewMagickWand();  
    status=MagickReadImage(magick_wand, (char *)imageFile);
    if (status == MagickFalse){
      ThrowWandException(magick_wand);
    }
    /*
      Turn the images into a thumbnail sequence.
    */
//    MagickResetIterator(magick_wand);
//    while (MagickNextImage(magick_wand) != MagickFalse){
        MagickSetFormat(magick_wand, (char *)format);
        MagickSetImageCompression(magick_wand, compressionType);
        MagickSetImageCompressionQuality(magick_wand, compressionRate);
//    }

    /*
      Write the image then destroy it.
    */
    status=MagickWriteImages(magick_wand, (char *)outputImage, MagickTrue);
    if (status == MagickFalse)
      ThrowWandException(magick_wand);
    magick_wand=DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    return status;
}

static void b_magickCompressImage1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[4]; // imageFile
    pntr val2 = argstack[3]; // outputImage
    pntr val3 = argstack[2]; // format
    pntr val4 = argstack[1]; // compressionType
    pntr val5 = argstack[0]; // compressionRate
    int badtype;

    /* the result value to be return */
    int result;

    char *imageFile;
    char *outputImage;
    char *format;
    int compressionType;
    double compressionRate;

    /* Check validity of each parameter */
    CHECK_ARG(4, CELL_CONS);
    CHECK_ARG(3, CELL_CONS);
    CHECK_ARG(2, CELL_CONS);
    CHECK_ARG(1, CELL_NUMBER);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &imageFile)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val2, &outputImage)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val3, &format)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    compressionType = pntrdouble(val4);
    compressionRate = pntrdouble(val5);

    /* Call the method and get the return value */
    result = magickCompressImage(imageFile, outputImage, format, compressionType, compressionRate);

    /* Translate the resultant pntr to be return */
    pntr p_result;
    set_pntrdouble(p_result, result);

    /* set the return value */
    argstack[0] = p_result;
}

int magickUnsharpMaskImage(const char *imageFile, const char *outputImage, double radius, double sigma, double amount, double threshold){
    MagickBooleanType status;

    MagickWand *magick_wand;

    /*
      Read an image.
    */
    MagickWandGenesis();
    magick_wand=NewMagickWand();  
    status=MagickReadImage(magick_wand, (char *)imageFile);
    if (status == MagickFalse){
      ThrowWandException(magick_wand);
    }
    /*
      Turn the images into a thumbnail sequence.
    */
//    MagickResetIterator(magick_wand);
//    while (MagickNextImage(magick_wand) != MagickFalse)
      MagickUnsharpMaskImage(magick_wand, radius, sigma, amount, threshold);
    /*
      Write the image then destroy it.
    */
    status=MagickWriteImages(magick_wand, (char *)outputImage, MagickTrue);
    if (status == MagickFalse)
      ThrowWandException(magick_wand);
    magick_wand=DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    return status;
}

static void b_magickUnsharpMaskImage1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[5]; // imageFile
    pntr val2 = argstack[4]; // outputImage
    pntr val3 = argstack[3]; // radius
    pntr val4 = argstack[2]; // sigma
    pntr val5 = argstack[1]; // amount
    pntr val6 = argstack[0]; // threshold
    int badtype;

    /* the result value to be return */
    int result;

    char *imageFile;
    char *outputImage;
    double radius;
    double sigma;
    double amount;
    double threshold;

    /* Check validity of each parameter */
    CHECK_ARG(5, CELL_CONS);
    CHECK_ARG(4, CELL_CONS);
    CHECK_ARG(3, CELL_NUMBER);
    CHECK_ARG(2, CELL_NUMBER);
    CHECK_ARG(1, CELL_NUMBER);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &imageFile)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val2, &outputImage)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    radius = pntrdouble(val3);
    sigma = pntrdouble(val4);
    amount = pntrdouble(val5);
    threshold = pntrdouble(val6);

    /* Call the method and get the return value */
    result = magickUnsharpMaskImage(imageFile, outputImage, radius, sigma, amount, threshold);

    /* Translate the resultant pntr to be return */
    pntr p_result;
    set_pntrdouble(p_result, result);

    /* set the return value */
    argstack[0] = p_result;
}

int magickThumbnailImage(const char *imageFile, const char *outputImage, int columns, int rows){
    MagickBooleanType status;

    MagickWand *magick_wand;

    /*
      Read an image.
    */
    MagickWandGenesis();
    magick_wand=NewMagickWand();  
    status=MagickReadImage(magick_wand, (char *)imageFile);
    if (status == MagickFalse){
      ThrowWandException(magick_wand);
    }
    /*
      Turn the images into a thumbnail sequence.
    */
    MagickResetIterator(magick_wand);
    while (MagickNextImage(magick_wand) != MagickFalse)
      MagickThumbnailImage(magick_wand, columns, rows);
    /*
      Write the image then destroy it.
    */
    status=MagickWriteImages(magick_wand, (char *)outputImage, MagickTrue);
    if (status == MagickFalse)
      ThrowWandException(magick_wand);
    magick_wand=DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    return status;
}

static void b_magickThumbnailImage1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[3]; // imageFile
    pntr val2 = argstack[2]; // outputImage
    pntr val3 = argstack[1]; // columns
    pntr val4 = argstack[0]; // rows
    int badtype;

    /* the result value to be return */
    int result;

    char *imageFile;
    char *outputImage;
    int columns;
    int rows;

    /* Check validity of each parameter */
    CHECK_ARG(3, CELL_CONS);
    CHECK_ARG(2, CELL_CONS);
    CHECK_ARG(1, CELL_NUMBER);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &imageFile)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val2, &outputImage)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    columns = pntrdouble(val3);
    rows = pntrdouble(val4);

    /* Call the method and get the return value */
    result = magickThumbnailImage(imageFile, outputImage, columns, rows);

    /* Translate the resultant pntr to be return */
    pntr p_result;
    set_pntrdouble(p_result, result);

    /* set the return value */
    argstack[0] = p_result;
}

int magickMotionBlurImage(const char *imageFile, const char *outputImage, double radius, double sigma, double angle){
    MagickBooleanType status;

    MagickWand *magick_wand;

    /*
      Read an image.
    */
    MagickWandGenesis();
    magick_wand=NewMagickWand();  
    status=MagickReadImage(magick_wand, (char *)imageFile);
    if (status == MagickFalse){
      ThrowWandException(magick_wand);
    }
    /*
      Turn the images into a thumbnail sequence.
    */
    MagickResetIterator(magick_wand);
    while (MagickNextImage(magick_wand) != MagickFalse)
      MagickMotionBlurImage(magick_wand, radius, sigma, angle);
    /*
      Write the image then destroy it.
    */
    status=MagickWriteImages(magick_wand, (char *)outputImage, MagickTrue);
    if (status == MagickFalse)
      ThrowWandException(magick_wand);
    magick_wand=DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    return status;
}

static void b_magickMotionBlurImage1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[4]; // imageFile
    pntr val2 = argstack[3]; // outputImage
    pntr val3 = argstack[2]; // radius
    pntr val4 = argstack[1]; // sigma
    pntr val5 = argstack[0]; // angle
    int badtype;

    /* the result value to be return */
    int result;

    char *imageFile;
    char *outputImage;
    double radius;
    double sigma;
    double angle;

    /* Check validity of each parameter */
    CHECK_ARG(4, CELL_CONS);
    CHECK_ARG(3, CELL_CONS);
    CHECK_ARG(2, CELL_NUMBER);
    CHECK_ARG(1, CELL_NUMBER);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &imageFile)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val2, &outputImage)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    radius = pntrdouble(val3);
    sigma = pntrdouble(val4);
    angle = pntrdouble(val5);

    /* Call the method and get the return value */
    result = magickMotionBlurImage(imageFile, outputImage, radius, sigma, angle);

    /* Translate the resultant pntr to be return */
    pntr p_result;
    set_pntrdouble(p_result, result);

    /* set the return value */
    argstack[0] = p_result;
}

int magickModulateImage(const char *imageFile, const char *outputImage, double brightness, double saturation, double hue){
    MagickBooleanType status;

    MagickWand *magick_wand;

    /*
      Read an image.
    */
    MagickWandGenesis();
    magick_wand=NewMagickWand();  
    status=MagickReadImage(magick_wand, (char *)imageFile);
    if (status == MagickFalse){
      ThrowWandException(magick_wand);
    }
    /*
      Turn the images into a thumbnail sequence.
    */
    MagickResetIterator(magick_wand);
    while (MagickNextImage(magick_wand) != MagickFalse)
      MagickModulateImage(magick_wand, brightness, saturation, hue);
    /*
      Write the image then destroy it.
    */
    status=MagickWriteImages(magick_wand, (char *)outputImage, MagickTrue);
    if (status == MagickFalse)
      ThrowWandException(magick_wand);
    magick_wand=DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    return status;
}

static void b_magickModulateImage1(task *tsk, pntr *argstack)
{
    /* pointers to the parameters */
    pntr val1 = argstack[4]; // imageFile
    pntr val2 = argstack[3]; // outputImage
    pntr val3 = argstack[2]; // brightness
    pntr val4 = argstack[1]; // saturation
    pntr val5 = argstack[0]; // hue
    int badtype;

    /* the result value to be return */
    int result;

    char *imageFile;
    char *outputImage;
    double brightness;
    double saturation;
    double hue;

    /* Check validity of each parameter */
    CHECK_ARG(4, CELL_CONS);
    CHECK_ARG(3, CELL_CONS);
    CHECK_ARG(2, CELL_NUMBER);
    CHECK_ARG(1, CELL_NUMBER);
    CHECK_ARG(0, CELL_NUMBER);

    /* Initialize all arguments for this method */
    if( (badtype = array_to_string(val1, &imageFile)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    if( (badtype = array_to_string(val2, &outputImage)) > 0) {
        set_error(tsk, "string: argument is not a string (contains non-char: %s)", cell_types[badtype]);
        return;
    }

    brightness = pntrdouble(val3);
    saturation = pntrdouble(val4);
    hue = pntrdouble(val5);

    /* Call the method and get the return value */
    result = magickModulateImage(imageFile, outputImage, brightness, saturation, hue);

    /* Translate the resultant pntr to be return */
    pntr p_result;
    set_pntrdouble(p_result, result);

    /* set the return value */
    argstack[0] = p_result;
}



const extfunc extfunc_info[NUM_EXTFUNCS] = {
//// zzip functions
{ "zzip_dir_real",  1, 1, b_zzip_dir_real  },

//// zzip version
{ "zzip_version",     0, 0, b_zzip_version    },
{ "zzip_read_dirent", 1, 1, b_zzip_read_dirent},
{ "zzip_read",        1, 1, b_zzip_read	      },
{ "drawRectangles1",  4, 4, b_drawRectangles1  },
{ "enlargeRect1",     2, 2, b_enlargeRect1},
{ "magickResizeImage1", 6, 6, b_magickResizeImage1},
{ "magickRotateImage1", 3, 3, b_magickRotateImage1},
{ "magickChopImage1", 6, 6, b_magickChopImage1},
{ "magickCompressImage1", 5, 5, b_magickCompressImage1},
{ "magickUnsharpMaskImage1", 6, 6, b_magickUnsharpMaskImage1},
{ "magickThumbnailImage1", 4, 4, b_magickThumbnailImage1},
{ "magickMotionBlurImage1", 5, 5, b_magickMotionBlurImage1},
{ "magickModulateImage1", 5, 5, b_magickModulateImage1},
};
