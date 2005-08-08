<?xml version="1.0"?>
<xsl:transform xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xhtml="http://www.w3.org/1999/xhtml" version="2.0">
  <xsl:template name="t1">
    <xsl:value-of select="4 + foo(12)"/>
  </xsl:template>
  <xsl:template name="t2"/>
  <xsl:template name="t3">
    <xsl:value-of select="4 + foo(12)"/>
  </xsl:template>
  <xsl:template name="t4">
    <xsl:value-of select="1"/>
    <xsl:value-of select="0"/>
  </xsl:template>
  <xsl:template name="t5"/>
  <xsl:template name="t6">
    <xsl:param name="a"/>
  </xsl:template>
  <xsl:template name="t7">
    <xsl:param name="a"/>
    <xsl:param name="b" select="2"/>
    <xsl:param name="c">
      <xsl:value-of select="1"/>
    </xsl:param>
  </xsl:template>
  <xsl:template match="book"/>
  <xsl:template name="t8" match="book"/>
  <xsl:template match="book"/>
  <xsl:template match="book">
    <xsl:param name="a"/>
  </xsl:template>
  <xsl:template match="book">
    <xsl:param name="a"/>
    <xsl:param name="b" select="2"/>
    <xsl:param name="c">
      <xsl:value-of select="1"/>
    </xsl:param>
  </xsl:template>
  <xsl:template name="t9" match="book"/>
  <xsl:template name="t10" match="book">
    <xsl:param name="a"/>
  </xsl:template>
  <xsl:template name="t11" match="book">
    <xsl:param name="a"/>
    <xsl:param name="b" select="2"/>
    <xsl:param name="c">
      <xsl:value-of select="1"/>
    </xsl:param>
  </xsl:template>
  <xsl:function name="test">
    <xsl:variable name="a"/>
    <xsl:variable name="b:a"/>
    <xsl:variable name="a" select="4 + foo(12)"/>
    <xsl:variable name="a"/>
    <xsl:variable name="a">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:variable>
    <xsl:analyze-string select="$foo" regex="{$pattern}">
      <xsl:matching-substring>
        <xsl:value-of select="."/>
      </xsl:matching-substring>
    </xsl:analyze-string>
    <xsl:analyze-string select="$foo" regex="{$pattern}">
      <xsl:non-matching-substring>
        <xsl:value-of select="."/>
      </xsl:non-matching-substring>
    </xsl:analyze-string>
    <xsl:analyze-string select="$foo" regex="{$pattern}">
      <xsl:matching-substring>
        <xsl:value-of select="."/>
        <xsl:value-of select="4 + foo(12)"/>
      </xsl:matching-substring>
    </xsl:analyze-string>
    <xsl:analyze-string select="$foo" regex="{$pattern}">
      <xsl:non-matching-substring>
        <xsl:value-of select="."/>
        <xsl:value-of select="4 + foo(12)"/>
      </xsl:non-matching-substring>
    </xsl:analyze-string>
    <xsl:analyze-string select="$foo" regex="{$pattern}">
      <xsl:matching-substring>
        <xsl:value-of select="."/>
      </xsl:matching-substring>
      <xsl:non-matching-substring>
        <xsl:value-of select="."/>
      </xsl:non-matching-substring>
    </xsl:analyze-string>
    <xsl:analyze-string select="$foo" regex="{$pattern}">
      <xsl:matching-substring>
        <xsl:value-of select="."/>
        <xsl:value-of select="4 + foo(12)"/>
      </xsl:matching-substring>
      <xsl:non-matching-substring>
        <xsl:value-of select="."/>
        <xsl:value-of select="4 + foo(12)"/>
      </xsl:non-matching-substring>
    </xsl:analyze-string>
    <xsl:analyze-string select="$foo" regex="{$pattern}" flags="{$flags}">
      <xsl:matching-substring>
        <xsl:value-of select="."/>
      </xsl:matching-substring>
    </xsl:analyze-string>
    <xsl:apply-imports/>
    <xsl:apply-imports/>
    <xsl:apply-imports>
      <xsl:with-param name="a" select="2"/>
    </xsl:apply-imports>
    <xsl:apply-imports>
      <xsl:with-param name="a" select="2"/>
      <xsl:with-param name="b" select="2"/>
    </xsl:apply-imports>
    <xsl:apply-imports>
      <xsl:with-param name="a"/>
      <xsl:with-param name="b"/>
    </xsl:apply-imports>
    <xsl:apply-imports>
      <xsl:with-param name="a">
        <xsl:value-of select="2"/>
        <xsl:value-of select="4"/>
      </xsl:with-param>
    </xsl:apply-imports>
    <xsl:apply-templates/>
    <xsl:apply-templates select="book"/>
    <xsl:apply-templates mode="foo"/>
    <xsl:apply-templates mode="f:oo"/>
    <xsl:apply-templates select="book" mode="foo"/>
    <xsl:apply-templates>
      <xsl:sort select="@title"/>
    </xsl:apply-templates>
    <xsl:apply-templates>
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:apply-templates select="book">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:apply-templates mode="foo">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:apply-templates select="book" mode="foo">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:apply-templates>
      <xsl:with-param name="a" select="4"/>
    </xsl:apply-templates>
    <xsl:apply-templates>
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
    </xsl:apply-templates>
    <xsl:apply-templates select="book">
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
    </xsl:apply-templates>
    <xsl:apply-templates mode="foo">
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
    </xsl:apply-templates>
    <xsl:apply-templates select="book" mode="foo">
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
    </xsl:apply-templates>
    <xsl:apply-templates>
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:apply-templates select="book">
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:apply-templates mode="foo">
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:apply-templates select="book" mode="foo">
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:apply-templates/>
    <xsl:apply-templates select="book" mode="foo">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:apply-templates>
    <xsl:attribute name="someattr"/>
    <xsl:attribute name="{$somevar}"/>
    <xsl:attribute name="someattr" select="4 + foo(12)"/>
    <xsl:attribute name="{$somevar}" select="4 + foo(12)"/>
    <xsl:attribute name="someattr">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:attribute>
    <xsl:attribute name="{$somevar}">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:attribute>
    <xsl:attribute name="someattr"/>
    <xsl:attribute name="{$somevar}"/>
    <xsl:attribute name="someattr">
      <xsl:value-of select="2"/>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:attribute>
    <xsl:attribute name="{$somevar}">
      <xsl:value-of select="2"/>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:attribute>
    <xsl:call-template name="foo"/>
    <xsl:call-template name="f:oo"/>
    <xsl:call-template name="foo"/>
    <xsl:call-template name="foo">
      <xsl:with-param name="a" select="4"/>
    </xsl:call-template>
    <xsl:call-template name="foo">
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
    </xsl:call-template>
    <xsl:choose>
      <xsl:when test="$a &gt; $b">
        <xsl:value-of select="4 + foo(12)"/>
      </xsl:when>
    </xsl:choose>
    <xsl:choose>
      <xsl:when test="$a &gt; $b">
        <xsl:value-of select="4 + foo(12)"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:choose>
      <xsl:when test="$a &gt; $b">
        <xsl:value-of select="4 + foo(12)"/>
      </xsl:when>
      <xsl:when test="$a = $b">
        <xsl:value-of select="2"/>
        <xsl:value-of select="4 + foo(12)"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="1"/>
        <xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:comment select="4 + foo(12)"/>
    <xsl:comment>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:comment>
    <xsl:comment/>
    <xsl:comment>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:comment>
    <xsl:copy/>
    <xsl:copy>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:copy>
    <xsl:copy/>
    <xsl:copy>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:copy>
    <xsl:copy-of select="book/chapter"/>
    <someelem/>
    <xsl:element name="{$somevar}"/>
    <someelem/>
    <xsl:element name="{$somevar}"/>
    <someelem>
      <xsl:value-of select="4 + foo(12)"/>
    </someelem>
    <xsl:element name="{$somevar}">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:element>
    <someelem>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </someelem>
    <xsl:element name="{$somevar}">
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:element>
    <xsl:fallback/>
    <xsl:fallback>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:fallback>
    <xsl:fallback>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:fallback>
    <xsl:for-each select="book"/>
    <xsl:for-each select="book">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:for-each>
    <xsl:for-each select="book">
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:for-each>
    <xsl:for-each select="book">
      <xsl:sort select="@title"/>
    </xsl:for-each>
    <xsl:for-each select="book">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:for-each>
    <xsl:for-each select="book">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:for-each>
    <xsl:for-each select="book">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:for-each>
    <xsl:for-each-group select="book" group-by="@author">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:for-each-group>
    <xsl:for-each-group select="book" group-adjacent="@author">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:for-each-group>
    <xsl:for-each-group select="book" group-starting-with="@author">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:for-each-group>
    <xsl:for-each-group select="book" group-ending-with="@author">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:for-each-group>
    <xsl:for-each-group select="book" group-by="@author">
      <xsl:sort select="@title"/>
    </xsl:for-each-group>
    <xsl:for-each-group select="book" group-by="@author">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
    </xsl:for-each-group>
    <xsl:for-each-group select="book" group-by="@author">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:for-each-group>
    <xsl:for-each-group select="book" group-by="@author">
      <xsl:sort select="@title"/>
      <xsl:sort select="@author"/>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:for-each-group>
    <xsl:if test="$b &gt; $a"/>
    <xsl:if test="$b &gt; $a">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:if>
    <xsl:if test="$b &gt; $a">
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:if>
    <xsl:message/>
    <xsl:message/>
    <xsl:message>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:message>
    <xsl:message>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:message>
    <xsl:message select="'test'"/>
    <xsl:message select="'test'"/>
    <xsl:message select="'test'">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:message>
    <xsl:message select="'test'">
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:message>
    <xsl:message select="'test'" terminate="yes"/>
    <xsl:message select="'test'" terminate="{$somevar}"/>
    <xsl:message select="'test'" terminate="yes"/>
    <xsl:message select="'test'" terminate="{$somevar}"/>
    <xsl:message select="'test'" terminate="yes">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:message>
    <xsl:message select="'test'" terminate="{$somevar}">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:message>
    <xsl:message select="'test'" terminate="yes">
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:message>
    <xsl:message select="'test'" terminate="{$somevar}">
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:message>
    <xsl:message terminate="yes"/>
    <xsl:message terminate="{$somevar}"/>
    <xsl:message terminate="yes">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:message>
    <xsl:message terminate="{$somevar}">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:message>
    <xsl:namespace name="foo" select="'http://some.namespace'"/>
    <xsl:namespace name="foo"/>
    <xsl:namespace name="foo">
      <xsl:value-of select="'http://some.namespace'"/>
    </xsl:namespace>
    <xsl:namespace name="foo">
      <xsl:value-of select="'http://some'"/>
      <xsl:value-of select="'.namespace'"/>
    </xsl:namespace>
    <xsl:namespace name="{$somevar}" select="'http://some.namespace'"/>
    <xsl:namespace name="{$somevar}"/>
    <xsl:namespace name="{$somevar}">
      <xsl:value-of select="'http://some.namespace'"/>
    </xsl:namespace>
    <xsl:namespace name="{$somevar}">
      <xsl:value-of select="'http://some'"/>
      <xsl:value-of select="'.namespace'"/>
    </xsl:namespace>
    <xsl:next-match/>
    <xsl:next-match/>
    <xsl:next-match>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
      <xsl:fallback>
        <xsl:value-of select="'fallback2'"/>
      </xsl:fallback>
    </xsl:next-match>
    <xsl:next-match/>
    <xsl:next-match/>
    <xsl:next-match>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
      <xsl:fallback>
        <xsl:value-of select="'fallback2'"/>
      </xsl:fallback>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:with-param name="a" select="4"/>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:with-param name="a" select="4"/>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:with-param name="a" select="4"/>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:with-param name="a" select="4"/>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
      <xsl:fallback>
        <xsl:value-of select="'fallback2'"/>
      </xsl:fallback>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
    </xsl:next-match>
    <xsl:next-match>
      <xsl:with-param name="a" select="4"/>
      <xsl:with-param name="b">
        <xsl:value-of select="2"/>
      </xsl:with-param>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
      <xsl:fallback>
        <xsl:value-of select="'fallback2'"/>
      </xsl:fallback>
    </xsl:next-match>
    <xsl:perform-sort>
      <xsl:sort select="@author"/>
    </xsl:perform-sort>
    <xsl:perform-sort>
      <xsl:sort select="@author"/>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:perform-sort>
    <xsl:perform-sort>
      <xsl:sort select="@author"/>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:perform-sort>
    <xsl:perform-sort>
      <xsl:sort select="@author"/>
      <xsl:sort select="@title"/>
    </xsl:perform-sort>
    <xsl:perform-sort>
      <xsl:sort select="@author"/>
      <xsl:sort select="@title"/>
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:perform-sort>
    <xsl:perform-sort>
      <xsl:sort select="@author"/>
      <xsl:sort select="@title"/>
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:perform-sort>
    <xsl:perform-sort select="book">
      <xsl:sort select="@author"/>
      <xsl:sort select="@title"/>
    </xsl:perform-sort>
    <xsl:perform-sort select="book">
      <xsl:sort select="@author"/>
      <xsl:sort select="@title"/>
    </xsl:perform-sort>
    <xsl:perform-sort select="book">
      <xsl:sort select="@author"/>
      <xsl:sort select="@title"/>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
    </xsl:perform-sort>
    <xsl:perform-sort select="book">
      <xsl:sort select="@author"/>
      <xsl:sort select="@title"/>
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
      <xsl:fallback>
        <xsl:value-of select="'fallback2'"/>
      </xsl:fallback>
    </xsl:perform-sort>
    <xsl:processing-instruction name="somepi" select="4 + foo(12)"/>
    <xsl:processing-instruction name="somepi"/>
    <xsl:processing-instruction name="somepi">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:processing-instruction>
    <xsl:processing-instruction name="somepi">
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:processing-instruction>
    <xsl:processing-instruction name="{$somevar}" select="4 + foo(12)"/>
    <xsl:processing-instruction name="{$somevar}"/>
    <xsl:processing-instruction name="{$somevar}">
      <xsl:value-of select="4 + foo(12)"/>
    </xsl:processing-instruction>
    <xsl:processing-instruction name="{$somevar}">
      <xsl:value-of select="1"/>
      <xsl:value-of select="0"/>
    </xsl:processing-instruction>
    <xsl:sequence select="4 + foo(12)"/>
    <xsl:sequence select="4 + foo(12)"/>
    <xsl:sequence select="4 + foo(12)">
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
    </xsl:sequence>
    <xsl:sequence select="4 + foo(12)">
      <xsl:fallback>
        <xsl:value-of select="'fallback1'"/>
      </xsl:fallback>
      <xsl:fallback>
        <xsl:value-of select="'fallback2'"/>
      </xsl:fallback>
    </xsl:sequence>
    <xsl:text>foo</xsl:text>
    <xsl:value-of select="4 + foo(12)"/>
    <xsl:value-of select="4 + foo(12)" separator="{string-join('sep' + other())}"/>
    <xsl:value-of/>
    <xsl:value-of separator="{string-join('sep' + other())}"/>
    <xsl:value-of>
      <xsl:copy-of select="text()"/>
    </xsl:value-of>
    <xsl:value-of separator="{string-join('sep' + other())}">
      <xsl:copy-of select="text()"/>
    </xsl:value-of>
    <xsl:value-of>
      <xsl:copy-of select="../text()"/>
      <xsl:copy-of select="text()"/>
    </xsl:value-of>
    <xsl:value-of separator="{string-join('sep' + other())}">
      <xsl:copy-of select="../text()"/>
      <xsl:copy-of select="text()"/>
    </xsl:value-of>
  </xsl:function>
</xsl:transform>
