<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0">
    <xsl:output method="text"></xsl:output>
  
	<!--Global variable definition: newline-->
	<xsl:variable name="NEWLINE">
	   <xsl:text>
</xsl:text>
    </xsl:variable>
	<xsl:variable name="INDENT">
	    <xsl:value-of select="'    '"></xsl:value-of></xsl:variable>

	<xsl:template match="/">
      <xsl:apply-templates select="/FUNCDEF"></xsl:apply-templates>
      <xsl:apply-templates select="/FUNCDEF/INTERFACE/METHOD"></xsl:apply-templates></xsl:template>
	

	<!--Gen: struct definition, e.g. typedef struct FileInfo FileInfo-->
    <xsl:template match="/FUNCDEF" >
    <xsl:call-template name="genStructTypedef">
        <xsl:with-param name="structs" select="."></xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="structsCreator">
        <xsl:with-param name="structs" select="."></xsl:with-param>
    </xsl:call-template>
    </xsl:template>
    
    <!--Gen: method-->
    <xsl:template match="/FUNCDEF/INTERFACE/METHOD">
        <xsl:call-template name="genMethodHead">
        <xsl:with-param name="method" select="."></xsl:with-param>
        </xsl:call-template>
        <xsl:value-of select="concat($NEWLINE, '{', $NEWLINE)"></xsl:value-of>
        <xsl:value-of select="concat($INDENT, '/* pointers to the parameters */', $NEWLINE)"></xsl:value-of>
        
        <xsl:call-template name="genPntr">
        <xsl:with-param name="method" select="."></xsl:with-param>
        <xsl:with-param name="paramPos" select="1"></xsl:with-param>
        </xsl:call-template>
        <xsl:value-of select="concat('    int badtype;', $NEWLINE)"></xsl:value-of>
        <xsl:call-template name="genVariables">
        <xsl:with-param name="method" select="."></xsl:with-param>
        </xsl:call-template>
        
        <!--Gen: check arguments-->
        <xsl:call-template name="chkArg" >
        <xsl:with-param name="method" select="."></xsl:with-param>
        </xsl:call-template>
        
        <!--Gen: initialize arguments-->
        <xsl:call-template name="initArgs" >
        <xsl:with-param name="method" select="."></xsl:with-param>
        </xsl:call-template>
        <!--call the method-->
        <xsl:value-of select="concat($NEWLINE, $INDENT, '/* Call the method and get the return value */', $NEWLINE)"></xsl:value-of>
        <xsl:value-of select="concat($INDENT, ./RESULT/@name, ' = ', @name, '(')"></xsl:value-of>
        <xsl:for-each select="./PARAMETER">
            <!--gen the list of parameters for this method-->
            <xsl:choose>
                <xsl:when test="position() = last()">
                    <xsl:value-of select="@name"></xsl:value-of></xsl:when>
                <xsl:otherwise>
                    <xsl:value-of select="concat(@name, ', ')"></xsl:value-of>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:for-each>
        <xsl:value-of select="concat(');', $NEWLINE, $NEWLINE)"></xsl:value-of>
        
        <!--Gen: translate C data type to ELC data type-->
        <xsl:value-of select="concat($INDENT, '/* Translate the resultant pntr to be return */', $NEWLINE)"></xsl:value-of>
        <xsl:if test="not(starts-with(./RESULT/@type, 'struct'))">
            <xsl:value-of select="concat($INDENT, 'pntr p_', ./RESULT/@name, ';', $NEWLINE)"></xsl:value-of>
        </xsl:if>
          
        <xsl:call-template name="returnResult">
        <xsl:with-param name="method" select="."></xsl:with-param>
        </xsl:call-template>
        <!--free return struct pointer-->
        <xsl:if test='starts-with(./RESULT/@type, "struct")'>
            <xsl:value-of select="concat($NEWLINE, $INDENT, '/* Free the return struct */', $NEWLINE)"></xsl:value-of>
            <xsl:value-of select="concat($INDENT, 'free_', substring-after(./RESULT/@type, &quot; &quot;), '(', ./RESULT/@name, ');', $NEWLINE)"></xsl:value-of>
        </xsl:if>
        <xsl:value-of select="concat('}', $NEWLINE, $NEWLINE)"></xsl:value-of>
    </xsl:template>
      

    <!--Function: gen method head, e.g. static void b_readfile1(task *tsk, pntr *argstack)-->
    <xsl:template name="genMethodHead">
    <xsl:param name="method"></xsl:param>
        <xsl:variable name="f_method">
            <xsl:call-template name="needForced">
            <xsl:with-param name="method" select="$method"></xsl:with-param>
            <xsl:with-param name="paramPos" select="1"></xsl:with-param></xsl:call-template>
        </xsl:variable>
        <xsl:choose>
            <xsl:when test="$f_method='true'">
                <xsl:value-of select="concat('static void b_', $method/@name, '1(task *tsk, pntr *argstack)')"></xsl:value-of></xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="concat('static void b_', $method/@name, '(task *tsk, pntr *argstack)')"></xsl:value-of>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:template>

    <!--Function: generate pntr to argstack. e.g. pntr val0 = argstack[0]-->
    <xsl:template name="genPntr">
	<xsl:param name="method"></xsl:param>
	<xsl:param name="paramPos"></xsl:param>
        <xsl:variable name="numParam" select="count($method/PARAMETER)"></xsl:variable>
        <xsl:if test="$paramPos &lt;= $numParam">
            <xsl:value-of select="concat('    pntr val', $paramPos, ' = argstack[', $numParam - $paramPos, '];')"></xsl:value-of>
            <xsl:value-of select="concat(' // ', $method/PARAMETER[$paramPos]/@name, $NEWLINE)"></xsl:value-of>
            <xsl:call-template name="genPntr">
            <xsl:with-param name="method" select="$method"></xsl:with-param>
            <xsl:with-param name="paramPos" select="$paramPos + 1"></xsl:with-param>
            </xsl:call-template>
	   </xsl:if>
    </xsl:template>

    <!--Fucntion: recusively TEST if given method contains any its parameters contain String or struct, return xl:boolean-->
    <xsl:template name="needForced">
    <xsl:param name="method"></xsl:param>
    <xsl:param name="paramPos"></xsl:param>
    <xsl:if test="count($method/PARAMETER)&gt;0">
        <xsl:variable name="paramType" select="$method/PARAMETER[$paramPos]/@type"></xsl:variable><xsl:choose>
            <xsl:when test="$paramType='String' or starts-with($paramType, &quot;struct&quot;)">
                <xsl:value-of select="true()"></xsl:value-of></xsl:when>
            <xsl:otherwise>
                <xsl:choose>
                    <xsl:when test="$paramPos=count($method/PARAMETER)">
                        <xsl:value-of select="false()"></xsl:value-of></xsl:when>
                    <xsl:otherwise>
                        <xsl:call-template name="needForced">
                        <xsl:with-param name="method" select="$method"></xsl:with-param>
                        <xsl:with-param name="paramPos" select="$paramPos + 1"></xsl:with-param></xsl:call-template>
                    </xsl:otherwise>
                </xsl:choose>
            </xsl:otherwise>
        </xsl:choose></xsl:if>
    </xsl:template>
    
    <!--Function: gen variable definition, e.g. char *fileName;-->
    <xsl:template name="genVariables" >
    <xsl:param name="method"></xsl:param>
    <xsl:for-each select="$method/*">
        <xsl:if test="name(.)='RESULT'">
            <xsl:value-of select="concat($NEWLINE, $INDENT, '/* the result value to be return */', $NEWLINE)"></xsl:value-of></xsl:if>
        <xsl:choose>
            <xsl:when test="./@type='int'">
                <xsl:value-of select="concat($INDENT, 'int ', ./@name, ';') "></xsl:value-of></xsl:when>
            <xsl:when test="./@type='String'">
                <xsl:value-of select="concat($INDENT, 'char *', ./@name, ';') "></xsl:value-of></xsl:when>
            <xsl:when test='starts-with(./@type, "struct")'>
                <xsl:value-of select="concat($INDENT, substring-after(./@type, ' '), ' *', ./@name, ' = new_', substring-after(@type, ' '), '();') "></xsl:value-of></xsl:when>
            <xsl:when test="@type='double'">
                <xsl:value-of select="concat($INDENT, 'double ', ./@name, ';') "></xsl:value-of></xsl:when>
        </xsl:choose>
        <xsl:value-of select="$NEWLINE"></xsl:value-of>
        <xsl:if test="name(.)='RESULT'">
            <xsl:value-of select="$NEWLINE"></xsl:value-of></xsl:if>
    </xsl:for-each>
    <xsl:value-of select="$NEWLINE"></xsl:value-of>
    </xsl:template>

    <!--Function: gen struct typedef, e.g. typedef struct FileInfo FileInfo-->
    <xsl:template name="genStructTypedef" >
    <xsl:param name="structs"></xsl:param>
        <xsl:for-each select="$structs/STRUCT">
            <xsl:value-of select="concat('typedef struct ', substring-after(@type, &quot; &quot;), ' ', substring-after(@type, &quot; &quot;), ';', $NEWLINE)"></xsl:value-of>
        </xsl:for-each>
        <xsl:value-of select="$NEWLINE"></xsl:value-of>
        <xsl:for-each select="$structs/STRUCT">
            <xsl:call-template name="genCStruct">
            <xsl:with-param name="struct" select="."></xsl:with-param></xsl:call-template>
        </xsl:for-each>
    <xsl:value-of select="$NEWLINE"></xsl:value-of>
</xsl:template>

    <!--Function: gen C struct, e.g. struct FileInfo{char *fileName, int lenth}-->
    <xsl:template name="genCStruct" >
    <xsl:param name="struct"></xsl:param>
        <xsl:value-of select="concat(normalize-space($struct/@type), ' {', $NEWLINE)"></xsl:value-of>
        <xsl:for-each select="$struct/*">
            <xsl:choose>
                <xsl:when test="@type='int'">
                    <xsl:value-of select="concat($INDENT, 'int ', @name, ';', $NEWLINE)"></xsl:value-of></xsl:when>
                <xsl:when test="@type='double'">
                    <xsl:value-of select="concat($INDENT, 'double ', @name, ';', $NEWLINE)"></xsl:value-of></xsl:when>
                <xsl:when test="@type='String'">
                    <xsl:value-of select="concat($INDENT, 'char *', @name, ';', $NEWLINE)"></xsl:value-of></xsl:when>
                <xsl:when test='starts-with(@type, "struct")'>
                    <xsl:value-of select="concat($INDENT, substring-after(@type, &quot; &quot;), ' *', @name, ';', $NEWLINE)"></xsl:value-of></xsl:when>
	       </xsl:choose>
        </xsl:for-each>
        <xsl:value-of select="concat('};', $NEWLINE, $NEWLINE)"></xsl:value-of>
    </xsl:template>

    <!--Function: gen struct constructor and destructor, e.g. Rectangle *new_Rectangle(); free_Rectangle() and its definition-->
    <xsl:template name="structsCreator" >
    <xsl:param name="structs"></xsl:param>
    <!--Declear these struct constructors and destructors-->
        <xsl:value-of select="concat($NEWLINE, '/* Declear the struct constructor and destructor */', $NEWLINE)" ></xsl:value-of>
        <xsl:for-each select="$structs/STRUCT">
            <xsl:value-of select="concat(substring-after(@type, ' '), ' *new_', substring-after(@type, ' '), '();', $NEWLINE)"></xsl:value-of>
            <xsl:value-of select="concat('void free_', substring-after(@type, ' '), '(', substring-after(@type, ' '), ' *f_', substring-after(@type, ' '), ');', $NEWLINE)">
            </xsl:value-of>
        </xsl:for-each>
        <xsl:value-of select="concat($NEWLINE, '/* Definition of the constructors and destructors */', $NEWLINE)"></xsl:value-of>
        <xsl:for-each select="$structs/STRUCT">
            <xsl:value-of select="concat(substring-after(@type, ' '), ' *new_', substring-after(@type, ' '), '() {', $NEWLINE)"></xsl:value-of>
            <xsl:value-of select="concat($INDENT, substring-after(@type, ' '), ' *ret',  ' = malloc(sizeof(', substring-after(@type, ' '), '));', $NEWLINE)"></xsl:value-of>
            <xsl:value-of select="concat($INDENT, 'return ret;', $NEWLINE, '}', $NEWLINE)"></xsl:value-of>
            <!--gen free_Struct()-->
            <xsl:value-of select="concat('void free_', substring-after(@type, ' '), '(', substring-after(@type, ' '), ' *f_', substring-after(@type, ' '), ') {', $NEWLINE)" >
            </xsl:value-of>
            <!--free each struct inside current struct-->
            <xsl:for-each select="./STRUCTDEC">
                <xsl:value-of select="concat($INDENT, 'free_', substring-after(@type, ' '), '(f_', substring-after(../@type, ' '), '-&gt;', @name, ');', $NEWLINE)"></xsl:value-of>
            </xsl:for-each>
            	<xsl:value-of select="concat($INDENT, 'free(f_', substring-after(@type, ' '), ');', $NEWLINE, '}', $NEWLINE, $NEWLINE)"></xsl:value-of>
        </xsl:for-each>
        <xsl:value-of select="$NEWLINE"></xsl:value-of>
    </xsl:template>

    <!--Function: Check arguments, e.g.  CHECK_ARG(0, CELL_CONS)-->
    <xsl:template name="chkArg" >
    <xsl:param name="method"></xsl:param>
        <xsl:variable name="numParam" select="count($method/PARAMETER)"></xsl:variable>
        <xsl:value-of select="concat($INDENT, '/* Check validity of each parameter */', $NEWLINE)"></xsl:value-of>
        <xsl:for-each select="$method/PARAMETER">
            <xsl:choose>
                <xsl:when test="@type='int' or @type='double'">
                    <xsl:value-of select="concat($INDENT, 'CHECK_ARG(', $numParam - position(), ', CELL_NUMBER);', $NEWLINE )"></xsl:value-of></xsl:when>
                <xsl:when test="@type='String' or starts-with(@type, &quot;struct&quot;)">
                    <xsl:value-of select="concat($INDENT, 'CHECK_ARG(', $numParam - position(), ', CELL_CONS);', $NEWLINE )"></xsl:value-of>
                </xsl:when>
            </xsl:choose>
        </xsl:for-each>
        <xsl:value-of select="$NEWLINE"></xsl:value-of>
    </xsl:template>

    <!--Function: initialize arguments, e.g. userName = "SmAxlL"; xPos = 10;-->
    <xsl:template name="initArgs" >
    <xsl:param name="method"></xsl:param>
    <xsl:value-of select="concat($INDENT, '/* Initialize all arguments for this method */', $NEWLINE)"></xsl:value-of>
    <xsl:for-each select="$method/PARAMETER">
        <xsl:choose>
            <xsl:when test="@type='int' or @type='double'">
                <xsl:value-of select="concat($INDENT, @name, ' = pntrdouble(val', position(), ');', $NEWLINE) "></xsl:value-of></xsl:when>
            <xsl:when test="@type='String'">
                <xsl:value-of select="concat($INDENT, 'if( (badtype = array_to_string(val', position(), ', &amp;', @name, ')) &gt; 0) {', $NEWLINE) "></xsl:value-of>
                <xsl:value-of select="concat($INDENT, $INDENT, 'set_error(tsk, &quot;string: argument is not a string (contains non-char: %s)&quot;, cell_types[badtype]);', $NEWLINE)"></xsl:value-of>
                <xsl:value-of select="concat($INDENT, $INDENT, 'return;', $NEWLINE)"></xsl:value-of>
                <xsl:value-of select="concat($INDENT, '}', $NEWLINE, $NEWLINE)"></xsl:value-of></xsl:when>
            <xsl:when test="starts-with(@type, 'struct')">
                <xsl:variable name="structType" select="normalize-space(@type)"></xsl:variable>
                <xsl:value-of select="concat($NEWLINE, $INDENT, '/* Initialize the struct: ', substring-after(@type, &quot; &quot;), ' */', $NEWLINE)"></xsl:value-of>
                <xsl:call-template name="initStruct">
                    <xsl:with-param name="struct" select="/FUNCDEF/STRUCT[normalize-space(@type)=$structType]"></xsl:with-param>
                    <xsl:with-param name="structName" select="@name"></xsl:with-param>
                    <xsl:with-param name="rootPntr" select="concat('val', position())"></xsl:with-param></xsl:call-template>
                <xsl:value-of select="concat($NEWLINE, $INDENT, '/* end Initialization of ', normalize-space(@type), ' */', $NEWLINE)"></xsl:value-of>
            </xsl:when>
        </xsl:choose>
    </xsl:for-each>
    </xsl:template>
    
    <!--Function: initialize all c struct arguments. e.g. Rectangle->width=10, col->red =100;-->
    <xsl:template name="initStruct" >
    <xsl:param name="struct"></xsl:param>
    <xsl:param name="structName"></xsl:param>
    <xsl:param name="rootPntr"></xsl:param>
    <xsl:for-each select="$struct/*">
        <xsl:choose>
            <xsl:when test="@type='int' or @type = 'double'">
                <xsl:value-of select="concat($INDENT, 'pntr ', $structName, '_val', position(), ' = ')"></xsl:value-of>
                <xsl:call-template name="getElement">
                    <xsl:with-param name="rootCONS" select="$rootPntr"></xsl:with-param>
                    <xsl:with-param name="pos" select="position()"></xsl:with-param>
                </xsl:call-template>
                <xsl:value-of select="concat(';', $NEWLINE)"></xsl:value-of>
                <xsl:value-of select="concat($INDENT, $structName, '-&gt;', @name, ' = pntrdouble(', $structName, '_val', position(), ');', $NEWLINE)"></xsl:value-of></xsl:when>
            <xsl:when test="@type='String'">
                <xsl:value-of select="concat($INDENT, 'pntr ', $structName, '_val', position(), ' = ')"></xsl:value-of>
                <xsl:call-template name="getElement">
                    <xsl:with-param name="rootCONS" select="$rootPntr"></xsl:with-param>
                    <xsl:with-param name="pos" select="position()"></xsl:with-param></xsl:call-template>
                <xsl:value-of select="concat(';', $NEWLINE)"></xsl:value-of>
            
                <xsl:value-of select="concat($INDENT, 'if( (badtype = array_to_string(', $structName, '_val', position(), ', &amp;(', $structName, '-&gt;', @name, ') )) &gt; 0) {', $NEWLINE) "></xsl:value-of>
                <xsl:value-of select="concat($INDENT, $INDENT, 'set_error(tsk, &quot;string: argument is not a string (contains non-char: %s)&quot;, cell_types[badtype]);', $NEWLINE)"></xsl:value-of>
                <xsl:value-of select="concat($INDENT, $INDENT, 'return;', $NEWLINE)"></xsl:value-of>
                <xsl:value-of select="concat($INDENT, '}', $NEWLINE, $NEWLINE)"></xsl:value-of>
            </xsl:when>
            <xsl:when test='starts-with(@type, "struct")'>
                <xsl:value-of select="concat($NEWLINE, $INDENT, '/* Initialize another struct: ', substring-after(@type, &quot; &quot;), '*/', $NEWLINE)"></xsl:value-of>
                <!--define the struct, e.g. Color rect_col = new_Color();-->
                <xsl:value-of select="concat($INDENT, substring-after(@type, &quot; &quot;), ' *', $structName, '_', @name, ' = new_', substring-after(@type, &quot; &quot;), '( );', $NEWLINE)"></xsl:value-of>
                <!--initialize the original struct member, e.g. rect->col = rect_col;-->
                <xsl:value-of select="concat($INDENT, $structName, '-&gt;', @name, ' = ', $structName, '_', @name, ';', $NEWLINE)" >
                </xsl:value-of>
                <!--new root pntr for the struct inside-->
                <xsl:value-of select="concat($INDENT, '/* new root pntr for ', @type, ' */', $NEWLINE)" ></xsl:value-of>
                <xsl:value-of select="concat($INDENT, 'pntr ', $structName, '_', @name, '_val = ')"></xsl:value-of>
                <xsl:call-template name="getElement">
                    <xsl:with-param name="rootCONS" select="$rootPntr"></xsl:with-param>
                    <xsl:with-param name="pos" select="position()"></xsl:with-param>
                </xsl:call-template>
                <xsl:value-of select="concat(';', $NEWLINE)"></xsl:value-of>
                <xsl:variable name="newStructType" select="normalize-space(@type)"></xsl:variable>
                <xsl:variable name="newRootPntr" select="concat($structName, '_', @name, '_val')"></xsl:variable>

                <xsl:call-template name="initStruct">
                    <xsl:with-param name="struct" select="/FUNCDEF/STRUCT[normalize-space(@type)=$newStructType]"></xsl:with-param>
                    <xsl:with-param name="structName" select="concat($structName, '_', @name)"></xsl:with-param>
                    <xsl:with-param name="rootPntr" select="$newRootPntr"></xsl:with-param>
                </xsl:call-template>                
            </xsl:when>
        </xsl:choose>
    </xsl:for-each>
    </xsl:template>

    <!--Function:  find target in a cons tree, e.g. head(tsk, tail(tsk, tail(tsk pntr)))-->
    <xsl:template name="getElement">
        <xsl:param name="rootCONS"></xsl:param>
        <xsl:param name="pos"></xsl:param>
        <xsl:if test="$pos &gt; 0">
            <xsl:value-of select="'head(tsk, '"></xsl:value-of>
            <xsl:call-template name="genTails">
                <xsl:with-param name="rootCONS" select="$rootCONS"></xsl:with-param>
                <xsl:with-param name="numTails" select="$pos - 1"></xsl:with-param>
            </xsl:call-template>
            <xsl:value-of select="')'"></xsl:value-of>
        </xsl:if>
    </xsl:template>

    <!--Function: recursively generate certain numbers of tail, e.g. (tail tsk, (tail (tsk, tail(tsk pntr)))))-->
    <xsl:template name="genTails" >
    <xsl:param name="rootCONS"></xsl:param>
    <xsl:param name="numTails"></xsl:param>
    <xsl:choose>
        <xsl:when test="$numTails = 0">
            <xsl:value-of select="$rootCONS"></xsl:value-of></xsl:when>
        <xsl:when test="$numTails &gt; 0">
            <xsl:value-of select="'tail(tsk, '"></xsl:value-of>
            <xsl:call-template name="genTails">
                <xsl:with-param name="rootCONS" select="$rootCONS"></xsl:with-param>
                <xsl:with-param name="numTails" select="$numTails - 1"></xsl:with-param>
            </xsl:call-template>
            <xsl:value-of select="')'"></xsl:value-of>
        </xsl:when>
    </xsl:choose>
    </xsl:template>
    
    <!--Function: return result, e.g. argstack[0] = p_rect_return;-->
    <xsl:template name="returnResult" >
    <xsl:param name="method"></xsl:param>
        <xsl:variable name="result" select="$method/RESULT"></xsl:variable>
        <xsl:choose>
            <xsl:when test="$result/@type = 'int' or $result/@type = 'double'">
                <xsl:value-of select="concat($INDENT, 'set_pntrdouble(p_', $result/@name, ', ', $result/@name, ');', $NEWLINE, $NEWLINE)"></xsl:value-of>
            </xsl:when>
            <xsl:when test="$result/@type = 'String'">
                <xsl:value-of select="concat($INDENT, 'p_', $result/@name, ' = string_to_array(tsk, ', $result/@name, ');', $NEWLINE)"></xsl:value-of>
            </xsl:when>
            <xsl:when test="starts-with($result/@type, 'struct')">
                <xsl:value-of select="concat($NEWLINE, $INDENT, '/* Translate C struct to ELC struct */', $NEWLINE)"></xsl:value-of>
                <xsl:variable name="structType" select="normalize-space($result/@type)"></xsl:variable>
                <xsl:call-template name="structConversion">
                <xsl:with-param name="spName" select="$result/@name"></xsl:with-param>
                <xsl:with-param name="paramName" select="$result/@name"></xsl:with-param>
                <xsl:with-param name="struct" select="/FUNCDEF/STRUCT[normalize-space(@type) = $structType]"></xsl:with-param>
                </xsl:call-template>
            </xsl:when>
        </xsl:choose>
        <xsl:value-of select="concat($INDENT, '/* set the return value */', $NEWLINE)"></xsl:value-of>
        <xsl:value-of select="concat($INDENT, 'argstack[0] = p_', $result/@name, ';', $NEWLINE)"></xsl:value-of>
    </xsl:template>

    <!--Function: translate C struct to ELC struct, e.g. pntr p_originalRect = make_cons(tsk, p_originalRect_width ...-->
    <xsl:template name="structConversion" >
        <xsl:param name="spName"><!--struct parameter name, which is like, rect->col->....--></xsl:param>
        <xsl:param name="paramName"></xsl:param>
        <xsl:param name="struct"></xsl:param>
        <!--recursively invoke every struct inside before itself to be generated-->
        <xsl:for-each select="$struct/*[name()='STRUCTDEC' ]">
            <xsl:variable name="newStructType" select="normalize-space(./@type)" >
            </xsl:variable>
            <xsl:call-template name="structConversion">
                <xsl:with-param name="spName" select="concat($spName, '-&gt;', @name)" ></xsl:with-param>
                <xsl:with-param name="paramName" select="concat($paramName, '_', @name)" ></xsl:with-param>
                <xsl:with-param name="struct" select="/FUNCDEF/STRUCT[normalize-space(@type) = $newStructType]" ></xsl:with-param>
            </xsl:call-template>

        </xsl:for-each>
        <xsl:value-of select="concat($NEWLINE, $INDENT, '/* pntr for ', normalize-space($struct/@type), '*/', $NEWLINE)" ></xsl:value-of>
        <!--Gen: pntr to the struct itself-->
        <xsl:for-each select="$struct/*[name()='STRUCTCOM' ]">
            <xsl:choose>
                <xsl:when test="@type = 'String'">
                    <xsl:value-of select="concat($INDENT, 'pntr p_', $paramName, '_', @name, ' = string_to_array(tsk, ', $spName, '-&gt;', @name, ');', $NEWLINE)" ></xsl:value-of></xsl:when>
                <xsl:when test="@type = 'int' or @type = 'double'">
                    <xsl:value-of select="concat($INDENT, 'pntr p_', $paramName, '_', @name, ';', $NEWLINE)"></xsl:value-of>
                    <xsl:value-of select="concat($INDENT, 'set_pntrdouble(p_', $paramName, '_', @name, ', ', $spName, '-&gt;', @name, ');', $NEWLINE)"></xsl:value-of>
                </xsl:when>
            </xsl:choose>
        </xsl:for-each>
        <!--Gen: root pntr for this struct-->
        <xsl:value-of select="concat($INDENT, '/* the root pntr for ', normalize-space($struct/@type), ' */', $NEWLINE)" ></xsl:value-of>
        <xsl:value-of select="concat($INDENT, 'pntr p_', $paramName, ' = ')"></xsl:value-of>
        <xsl:for-each select="$struct/*">
            <xsl:value-of select="concat('make_cons(tsk, p_', $paramName, '_', @name, ', ')"></xsl:value-of>    
        </xsl:for-each>
    
        <xsl:value-of select="'tsk-&gt;globnilpntr'"></xsl:value-of>
        <xsl:call-template name="genBrackets">
        <xsl:with-param name="count" select="count($struct/*)"></xsl:with-param>
        </xsl:call-template>
        <xsl:value-of select="concat(';', $NEWLINE, $NEWLINE)"></xsl:value-of>
    </xsl:template>

    <!--Function: gen brackets, e.g. ))))))), the number is specified as parameter count-->
    <xsl:template name="genBrackets" >
    <xsl:param name="count"></xsl:param>
    <xsl:if test="$count &gt; 0">
        <xsl:value-of select="')'"></xsl:value-of>
        <xsl:call-template name="genBrackets">
            <xsl:with-param name="count" select="$count - 1"></xsl:with-param>
        </xsl:call-template>
    </xsl:if>
    </xsl:template>

</xsl:stylesheet>