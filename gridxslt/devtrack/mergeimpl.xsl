<?xml version="1.0"?>
<xsl:transform
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  xmlns:xhtml="http://www.w3.org/1999/xhtml"
  xmlns:fn="http://www.w3.org/2005/04/xpath-functions"
  xmlns:f="http://my.functions"
  xmlns:dt="http://gridxslt.sourceforge.net/devtrack"
  xmlns="http://gridxslt.sourceforge.net/devtrack"
  xpath-default-namespace="http://gridxslt.sourceforge.net/devtrack"
  exclude-result-prefixes="xsl xs xhtml fn f"
  version="2.0">

  <xsl:include href="util.xsl"/>

  <xsl:param name="implfile" as="xs:string" required="yes"/>

  <xsl:variable name="impl" select="doc($implfile)/implementation"/>

  <xsl:template match="/">
    <xsl:variable name="speclist" select="/dt:speclist"/>

    <xsl:variable name="specs"
      select="for $name in $speclist/dt:spec/@name
              return document(concat('output/',$name,'-details.xml'))/dt:spec"/>

    <xsl:for-each select="$specs">
      <xsl:variable name="spec" select="."/>
      <xsl:variable name="specname" select="@name"/>
      <xsl:result-document href="{concat('output/',$spec/@name,'-merged.xml')}">
        <xsl:copy>
          <xsl:sequence select="attribute::node()"/>
          <xsl:for-each select="dt:section">
            <xsl:copy>
              <xsl:sequence select="attribute::node()"/>
              <xsl:for-each select="dt:clause">
                <xsl:copy>
                  <xsl:sequence select="attribute::node()"/>
                  <xsl:variable name="thisname" select="@name"/>
                  <xsl:attribute name="status"
                    select="f:getstatus($impl/dt:clause[@spec=$specname and @name=$thisname])"/>
                  <xsl:copy-of
                    select="$impl/dt:clause[@spec=$specname and @name=$thisname]/node()"/>
                </xsl:copy>
              </xsl:for-each>
            </xsl:copy>
          </xsl:for-each>
        </xsl:copy>
      </xsl:result-document>
    </xsl:for-each>

  </xsl:template>

</xsl:transform>
