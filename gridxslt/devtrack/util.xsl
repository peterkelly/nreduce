<?xml version="1.0"?>
<xsl:transform
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  xmlns:xhtml="http://www.w3.org/1999/xhtml"
  xmlns:f="http://my.functions"
  xmlns:dt="http://gridxslt.sourceforge.net/devtrack"
  exclude-result-prefixes="xsl xs xhtml f"
  version="2.0">

  <xsl:variable name="stati" select="('supported','unsupported','partial','deferred','info')"/>

  <xsl:variable name="textlen" select="80"/>

  <xsl:function name="f:replace-whitespace" as="xs:string">
    <xsl:param name="str" as="xs:string"/>
    <xsl:param name="replacement" as="xs:string"/>
    <xsl:value-of select="replace(normalize-space($str),
                          '[ \r\n\t&#160;]+',$replacement,'sm')"/>
  </xsl:function>

  <xsl:function name="f:remove-whitespace" as="xs:string">
    <xsl:param name="str" as="xs:string"/>
    <xsl:value-of select="f:replace-whitespace($str,'')"/>
  </xsl:function>

  <xsl:function name="f:simplify-string" as="xs:string">
    <xsl:param name="str" as="xs:string"/>
    <xsl:value-of select="f:replace-whitespace($str,' ')"/>
  </xsl:function>

  <xsl:function name="f:getpartext" as="xs:string">
    <xsl:param name="str" as="xs:string"/>
    <xsl:value-of select="if (matches($str,'[^0-9]\.') and
                              string-length(replace($str,'([^0-9])\..*$','$1')) &lt;= $textlen) then
                            replace($str,'([^0-9])\..*$','$1')
                          else
                            substring($str,1,$textlen)"/>
  </xsl:function>

  <xsl:function name="f:getstatus">
    <xsl:param name="implclause"/>
    <xsl:choose>
      <xsl:when test="$implclause/dt:status='partial'">
        <xsl:value-of select="'partial'"/>
      </xsl:when>
      <xsl:when test="$implclause/dt:status='deferred'">
        <xsl:value-of select="'deferred'"/>
      </xsl:when>
      <xsl:when test="$implclause/dt:status='info'">
        <xsl:value-of select="'info'"/>
      </xsl:when>
      <xsl:when test="$implclause and (not($implclause/dt:status) or
                                       ($implclause/dt:status = 'supported'))">
        <xsl:value-of select="'supported'"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="'unsupported'"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:function>

</xsl:transform>
