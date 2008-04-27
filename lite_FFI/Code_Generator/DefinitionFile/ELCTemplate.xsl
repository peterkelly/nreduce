<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0">
    <xsl:output method="text"></xsl:output>
  
	<!--Global variable definition: newline-->
	<xsl:variable name="NEWLINE">
	   <xsl:text>
</xsl:text></xsl:variable>

    <xsl:template match = "/" >
        <xsl:apply-templates select = "/FUNCDEF/STRUCT" ></xsl:apply-templates>
        <xsl:apply-templates select="/FUNCDEF/INTERFACE/METHOD"></xsl:apply-templates>
    </xsl:template>
          
    <!--Gen methods in INTERFACE-->
    <xsl:template match='/FUNCDEF/INTERFACE/METHOD'>
        <xsl:if test="count(./PARAMETER) &gt; 0"><xsl:variable name="f_method">
            <xsl:call-template name="needForced">
                <xsl:with-param name="method" select="."></xsl:with-param>
                <xsl:with-param name="paramPos" select="1"></xsl:with-param></xsl:call-template></xsl:variable>
            <xsl:if test="$f_method='true'"><xsl:call-template name="forceMethod">
            <xsl:with-param name="method" select="."></xsl:with-param>
            </xsl:call-template><xsl:value-of select="$NEWLINE"></xsl:value-of></xsl:if>
            </xsl:if>
    </xsl:template>

	<!--ELC Gen: for each struct-->
    <xsl:template match="/FUNCDEF/STRUCT">
        <xsl:variable name="structType" select="./@type"></xsl:variable>
        <!--ELC GEN: get items from the object-->
        <xsl:for-each select="./*">
            <xsl:call-template name="getStructCom">
          	<xsl:with-param name="structType" select="$structType"></xsl:with-param>
          	<xsl:with-param name="structCom" select="."></xsl:with-param>
          	<xsl:with-param name="NO" select="position()-1"></xsl:with-param></xsl:call-template>
          	<xsl:value-of select="$NEWLINE"></xsl:value-of>
        </xsl:for-each>

        <!--ELC GEN: mk**** to generate ELC routine for end users to construct C struct in ELC programs-->
        <xsl:value-of select="concat('mk', substring-after($structType, &quot; &quot;), ' ')"></xsl:value-of>
        <xsl:for-each select="./*">
            <xsl:value-of select="concat(./@name, ' ')"></xsl:value-of>
        </xsl:for-each>
            <xsl:value-of select="'= '"></xsl:value-of>
            <xsl:call-template name="gen_mkStruct">
            <xsl:with-param name="struct" select="."></xsl:with-param>
            <xsl:with-param name="pos" select="1"></xsl:with-param></xsl:call-template>
            <xsl:value-of select="$NEWLINE">
        </xsl:value-of>

        <!--ELC GEN: Force evaluation of all struct in -->
        <xsl:value-of select="concat('force', substring-after($structType, &quot; &quot;), ' obj = (forcelist (mk', substring-after($structType, &quot; &quot;), ' ')"></xsl:value-of>
        <xsl:for-each select="./*">
            <xsl:call-template name="forceStructCom">
            <xsl:with-param name="structType" select="$structType"></xsl:with-param>
            <xsl:with-param name="structCom" select="."></xsl:with-param></xsl:call-template>
        </xsl:for-each>
        <xsl:value-of select="concat('))', $NEWLINE, $NEWLINE)"></xsl:value-of>
    </xsl:template>
          
    <!--Function: Generate forced strings, e.g. (forcelist str) , -->
    <xsl:template name="createForcelist">
    <xsl:param name="paramNode"></xsl:param>
	   <xsl:choose>
	       <xsl:when test="@type='String'">
	           <xsl:value-of select="concat('(forcelist ', ./@name, ') ')"></xsl:value-of>
	       </xsl:when>
	       <xsl:when test="starts-with(@type, 'struct')">
	           <xsl:value-of select="concat('(force', substring-after(@type, ' '), ' ', @name, ') ')"></xsl:value-of></xsl:when>
	       <xsl:otherwise>
	           <xsl:value-of select="concat(./@name, ' ')"></xsl:value-of></xsl:otherwise></xsl:choose>
    </xsl:template>

    <!--Function: Force evaluation of each struct component, e.g. forceFileInfo obj = ....-->
	<xsl:template name="forceStructCom">
	   <xsl:param name="structType"></xsl:param>
	   <xsl:param name="structCom"></xsl:param>
	       <xsl:choose>
	           <xsl:when test="$structCom/@type='String'">
	               <xsl:value-of select="concat('(forcelist (', substring-after($structType, &quot; &quot;), '_', $structCom/@name, ' obj)) ' )"></xsl:value-of></xsl:when>
	           <xsl:when test="local-name($structCom)='STRUCTDEC'">
	               <xsl:value-of select="concat('(force', substring-after($structCom/@type, &quot; &quot;), ' (', substring-after($structType, &quot; &quot;), '_', $structCom/@name, ' obj)) ' )"></xsl:value-of></xsl:when>
	           <xsl:otherwise>
	               <xsl:value-of select="concat('(', substring-after($structType, &quot; &quot;), '_', $structCom/@name, ' obj) ' )"></xsl:value-of>
	           </xsl:otherwise>
            </xsl:choose>
    </xsl:template>

    <!--Function: Retrive components of each struct: e.g. FileInfo_fileLen obj = (item 1 obj)-->
    <xsl:template name="getStructCom">
        <xsl:param name="structType"></xsl:param>
        <xsl:param name="structCom"></xsl:param>
        <xsl:param name="NO"></xsl:param>
            <xsl:value-of select="concat(substring-after($structType, &quot; &quot;), '_', $structCom/@name, ' obj = (item ', $NO, ' obj)' )"></xsl:value-of>
    </xsl:template>
    
    <!--Function: recursively generate: e.g. (cons item1 (cons item2... )))-->
    <xsl:template name="gen_mkStruct">
        <xsl:param name="struct"></xsl:param>
        <xsl:param name="pos"></xsl:param>
            <xsl:choose>
                <xsl:when test="count($struct/*)=$pos">
                    <xsl:value-of select="concat('(cons ', $struct/*[$pos]/@name, ' nil)' ) "></xsl:value-of></xsl:when>
                <xsl:otherwise>
                    <xsl:value-of select="concat('(cons ', $struct/*[$pos]/@name, ' ')"></xsl:value-of>
                    <xsl:call-template name="gen_mkStruct">
                        <xsl:with-param name="struct" select="$struct"></xsl:with-param>
                        <xsl:with-param name="pos" select="$pos + 1"></xsl:with-param></xsl:call-template>
                    <xsl:value-of select="')'"></xsl:value-of>
                </xsl:otherwise>
            </xsl:choose>
    </xsl:template>
    
    <!--Function: force the given method. e.g. read_file fileName = read_file1 (forcelist filename)-->
    <xsl:template name="forceMethod" >
    <xsl:param name="method"></xsl:param>
        <xsl:value-of select="concat($method/@name, ' ')"></xsl:value-of>
        <!--Gen: original parameter list-->
        <xsl:for-each select="$method/PARAMETER">
            <xsl:value-of select="concat(@name, ' ')"></xsl:value-of>
        </xsl:for-each>
        <xsl:value-of select="concat('= (', $method/@name, '1 ')"></xsl:value-of>
        <!--Gen: new parameter list, with forced strings and structs-->
        <xsl:for-each select="$method/PARAMETER" >
            <xsl:call-template name="createForcelist">
            <xsl:with-param name="paramNode" select="."></xsl:with-param></xsl:call-template>
        </xsl:for-each>
        <xsl:value-of select="')'"></xsl:value-of>
    </xsl:template>
    
    <!--Fucntion: recusively test if given method contains any its parameters contain String or struct, return xl:boolean-->
    <xsl:template name="needForced">
    <xsl:param name="method"></xsl:param>
    <xsl:param name="paramPos"></xsl:param>
        <xsl:variable name="paramType" select="$method/PARAMETER[$paramPos]/@type"></xsl:variable>
        <xsl:choose>
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
            </xsl:otherwise></xsl:choose>
    </xsl:template>

</xsl:stylesheet>
