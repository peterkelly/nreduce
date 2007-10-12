<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		xmlns:f="http://functions"
    xmlns:xs="http://www.w3.org/2001/XMLSchema"
		exclude-result-prefixes="f xs"
		version="2.0">

  <xsl:output method="xml" indent="yes"/>
  <xsl:strip-space elements="*"/>

  <xsl:function name="f:nfib">
    <xsl:param name="n"/>
    <xsl:sequence select="if ($n gt 1) then f:nfib($n - 2) + f:nfib($n - 1) else 1"/>
  </xsl:function>

  <xsl:template match="/">
    <result>
      <xsl:for-each select="1 to 32">
        <xsl:text>nfib(</xsl:text>
        <xsl:sequence select="."/>
        <xsl:text>) = </xsl:text>
        <xsl:sequence select="f:nfib(.)"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:for-each>
    </result>
  </xsl:template>

</xsl:stylesheet>
