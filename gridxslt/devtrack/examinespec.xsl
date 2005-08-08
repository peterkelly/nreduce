<?xml version="1.0"?>
<xsl:transform
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  xmlns:xhtml="http://www.w3.org/1999/xhtml"
  xmlns:f="http://my.functions"
  xmlns="http://gridxslt.sourceforge.net/devtrack"
  xpath-default-namespace="http://gridxslt.sourceforge.net/devtrack"
  exclude-result-prefixes="xsl xs xhtml f"
  version="2.0">

  <xsl:include href="util.xsl"/>

  <xsl:variable name="root" select="/"/>

  <xsl:function name="f:get-spec-url" as="xs:string">
    <xsl:param name="root"/>
    <xsl:variable name="latest" select="$root//xhtml:dt[string(.)='Latest version:']"/>
    <xsl:value-of select="$latest/following-sibling::xhtml:dd[1]//xhtml:a/@href"/>
  </xsl:function>

  <xsl:function name="f:get-spec-name" as="xs:string">
    <xsl:param name="root"/>
    <xsl:value-of select="replace(f:get-spec-url($root),'^.*/TR/([^/]+)/?','$1')"/>
  </xsl:function>

  <xsl:variable name="headers">
    <xsl:for-each select="//xhtml:div[@class='toc']">
      <xsl:for-each select=".//xhtml:a[@href]">
        <section number="{f:remove-whitespace(preceding-sibling::text()[1])}"
                 name="{substring-after(@href,'#')}"
                 title="{normalize-space(.)}"/>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:variable>

  <xsl:variable name="secfrags" select="$headers/section/@name"/>
  <xsl:variable name="secdivs"
    select="for $frag in $secfrags return $root//xhtml:a[@id=$frag]/ancestor::xhtml:div[1]"/>

  <xsl:variable name="tasks">
    <xsl:for-each select="$headers/section">
      <xsl:copy>
        <xsl:sequence select="attribute::node()"/>

        <xsl:variable name="frag" select="@name"/>
        <xsl:variable name="clauses"
          select="$root//xhtml:a[@id=$frag]/ancestor::xhtml:div[1]/element()
                  [not(some $of in $secdivs satisfies ($of is .)) and
                   count(.//xhtml:a[@id=$frag]) = 0]"/>

        <xsl:for-each select="$clauses">
          <xsl:variable name="str" select="f:simplify-string(string(.))"/>
          <clause name="{concat($frag,'-',position())}"
            text="{f:getpartext($str)}"/>
        </xsl:for-each>

      </xsl:copy>
    </xsl:for-each>
  </xsl:variable>

  <xsl:template match="/">
    <spec name="{f:get-spec-name(/)}" url="{f:get-spec-url(/)}" title="{//xhtml:title}">
      <xsl:copy-of select="$tasks"/>
    </spec>
  </xsl:template>

</xsl:transform>
