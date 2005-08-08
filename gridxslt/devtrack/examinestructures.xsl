<?xml version="1.0"?>
<xsl:transform
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xhtml="http://www.w3.org/1999/xhtml"
  xmlns:f="http://my.functions"
  xmlns:dt="http://gridxslt.sourceforge.net/devtrack"
  xmlns="http://gridxslt.sourceforge.net/devtrack"
  version="2.0">

  <xsl:include href="util.xsl"/>

  <xsl:template match="/">
    <spec name="xmlschema-1" url="http://www.w3.org/TR/xmlschema-1/"
      title="XML Schema Part 1: Structures Second Edition">

      <xsl:for-each select="//div2">
        <xsl:if test="count(.//constraintnote) > 0">
          <xsl:variable name="number">
            <xsl:number level="multiple" count="div1 | div2" format="1.1"/>
          </xsl:variable>

          <section number="{$number}" name="{@id}" title="{string(head)}">
            <xsl:for-each select=".//constraintnote">
              <xsl:variable name="cid" select="@id"/>
              <xsl:variable name="cname" select="string(head)"/>
              <xsl:variable name="constraint" select="."/>
              <xsl:choose>
                <xsl:when test="count(.//item) > 0">
                  <xsl:for-each select=".//item">
                    <xsl:variable name="fullid">
                      <xsl:value-of select="$cid"/>.<xsl:number level="multiple" count="item"/>
                    </xsl:variable>
                    <clause name="{$fullid}">
                      <xsl:variable name="text">
                        <xsl:value-of select="$cname"/> - 
                        <xsl:number level="multiple" count="item"/>
                      </xsl:variable>
                      <xsl:attribute name="text" select="f:simplify-string($text)"/>
                    </clause>
                  </xsl:for-each>
                </xsl:when>
                <xsl:otherwise>
                  <clause name="{$cid}" text="{$cname}"/>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:for-each>
          </section>

        </xsl:if>
      </xsl:for-each>

      <xsl:for-each select="//compdef[@name!='Example']">
        <xsl:variable name="scname" select="@name"/>
        <xsl:variable name="number">
          <xsl:number select="ancestor::div3"
            level="multiple" count="div1 | div2 | div3" format="1.1.1"/>
        </xsl:variable>
        <section number="{$number}" name="{$scname}" title="{ancestor::div3/head}">
          <xsl:for-each select="proplist/propdef">
            <xsl:variable name="propname" select="concat($scname,'{',@name,'}')"/>
            <clause name="{$propname}" text="{$propname}"/>
          </xsl:for-each>
        </section>
      </xsl:for-each>
    </spec>
  </xsl:template>

</xsl:transform>	
