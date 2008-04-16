<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0">
  <xsl:output method="text"></xsl:output> 
          <xsl:template match = "/" >
               <xsl:apply-templates select = "/FUNCDEF/INTERFACE/METHOD/PARAMETER" ></xsl:apply-templates>
          </xsl:template>
          <!--Generate ELC wrappers-->
          <xsl:template match='PARAMETER[@type="String"]'>
          	<!--Generate methods' name and corressonding parameters-->
          	<xsl:value-of select="../@name"></xsl:value-of>
          	<xsl:text> </xsl:text>
          	<!--Generate a list of parameters-->
          	<xsl:for-each select="../PARAMETER">
          		<xsl:value-of select="./@name"></xsl:value-of>
          		<xsl:text> </xsl:text></xsl:for-each>
          	<xsl:text>= (</xsl:text><!--Method name (its parent), add 1 to form actual builtin function-->
          	<xsl:value-of select="../@name"></xsl:value-of>
          	<xsl:text>1 </xsl:text><!--Generate a new list of parameters with validated parameters--><xsl:for-each
          		select="../PARAMETER">
	<xsl:call-template name="createForcelist">
          		<xsl:with-param name="paramNode" select="."></xsl:with-param></xsl:call-template></xsl:for-each>
          	
          	<xsl:text>) 
</xsl:text></xsl:template>
          <xsl:template name="createForcelist">
          <!--Generate the string in the form of (forcelist str) or not, according to the given parameter node--><xsl:param name="paramNode"></xsl:param>
          	<xsl:choose>
          		<xsl:when test="@type='String'">
          			<xsl:text>(forcelist </xsl:text>
          			<xsl:value-of select="./@name"></xsl:value-of>
          			<xsl:text>) </xsl:text></xsl:when>
          		<xsl:otherwise>
          			<xsl:value-of select="./@name"></xsl:value-of></xsl:otherwise></xsl:choose></xsl:template>
</xsl:stylesheet>
