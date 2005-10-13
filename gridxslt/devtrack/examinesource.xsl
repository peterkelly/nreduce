<?xml version="1.0"?>
<xsl:transform
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  xmlns:xhtml="http://www.w3.org/1999/xhtml"
  xmlns:fn="http://www.w3.org/2005/04/xpath-functions"
  xmlns:f="http://my.functions"
  xmlns="http://gridxslt.sourceforge.net/devtrack"
  xpath-default-namespace="http://gridxslt.sourceforge.net/devtrack"
  exclude-result-prefixes="xsl xs xhtml fn f"
  version="2.0">

  <xsl:variable name="default-spec" select="''"/>

  <xsl:template match="/">

    <xsl:variable name="source-info-unmerged">
      <xsl:for-each select="/sourcefiles/file">

        <xsl:variable name="source" select="unparsed-text(@name,'UTF-8')"/>

        <xsl:analyze-string select="$source" regex="@implements\((.*?)\)(.*?)@end" flags="s">
          <xsl:matching-substring>
            <xsl:variable name="fullname" select="regex-group(1)"/>
            <xsl:variable name="spec" select="if (contains($fullname,':')) then
                                                substring-before($fullname,':')
                                              else
                                                $default-spec"/>
            <xsl:variable name="name" select="if (contains($fullname,':')) then
                                                substring-after($fullname,':')
                                              else
                                                $fullname"/>
            <clause spec="{$spec}" name="{$name}">
              <xsl:analyze-string select="regex-group(2)"
                regex="([a-zA-Z]+)\s*\{{\s*(.*?)\s*\}}" flags="s">
                <xsl:matching-substring>
                  <xsl:element name="{lower-case(regex-group(1))}">
                    <xsl:value-of select="regex-group(2)"/>
                  </xsl:element>
                </xsl:matching-substring>
              </xsl:analyze-string>
            </clause>
          </xsl:matching-substring>
        </xsl:analyze-string>

      </xsl:for-each>
    </xsl:variable>

    <xsl:variable name="source-info">
      <xsl:for-each-group select="$source-info-unmerged/clause" group-by="concat(@spec,':',@name)">

        <xsl:copy>
          <xsl:sequence select="attribute::node()"/>
          <xsl:for-each select="current-group()/node()">
            <xsl:copy-of select="."/>
          </xsl:for-each>
        </xsl:copy>
      </xsl:for-each-group>
    </xsl:variable>

    <implementation>
      <xsl:copy-of select="$source-info"/>
    </implementation>
    
  </xsl:template>

</xsl:transform>
