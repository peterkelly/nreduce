<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		xmlns:mandel="wsdl-http://localhost:8080/mandelbrot?WSDL"
		xmlns:f="http://functions"
		exclude-result-prefixes="mandel"
		version="2.0">

  <xsl:output method="xml" indent="yes"/>
  <xsl:strip-space elements="*"/>

  <xsl:function name="f:runmandel">
    <xsl:variable name="incr" select="0.1"/>
    <xsl:for-each select="-10 to 10">
      <xsl:variable name="y" select=". div 10"/>
      <xsl:for-each select="-15 to 5">
        <xsl:variable name="x" select=". div 10"/>
        <xsl:sequence select="mandel:domandel($x,$x+$incr,$y,$y+$incr,0.001)"/>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:function>

  <xsl:template match="/">
    <result>
      <xsl:value-of select="sum(f:runmandel())"/>
    </result>
  </xsl:template>

</xsl:stylesheet>
