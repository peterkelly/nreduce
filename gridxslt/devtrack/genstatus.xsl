<?xml version="1.0"?>
<xsl:transform
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  xmlns:fn="http://www.w3.org/2005/04/xpath-functions"
  xmlns:f="http://my.functions"
  xmlns:dt="http://gridxslt.sourceforge.net/devtrack"
  xmlns="http://www.w3.org/1999/xhtml"
  xpath-default-namespace="http://gridxslt.sourceforge.net/devtrack"
  exclude-result-prefixes="xsl xs fn f dt"
  version="2.0">

  <xsl:output method="xhtml"/>

  <xsl:include href="util.xsl"/>

  <xsl:template match="/">
    <xsl:variable name="tasks" select="/spec"/>
    <html>
      <head>
        <title>Development tracking</title>
        <link rel="stylesheet" type="text/css" href="style.css"/>
      </head>
      <body onload="window.top.frames['top'].update()">

        <h1 align="center"><xsl:value-of select="$tasks/@title"/></h1>

        <table>
          <tr>
            <th>Number</th>
            <th colspan="2">Name</th>
          </tr>

          <xsl:for-each select="$tasks/section">
            <xsl:variable name="frag" select="@name"/>

            <tr>
              <td>
                <xsl:value-of select="@number"/>
              </td>
              <td colspan="2">
                <b>
                  <xsl:value-of select="@title"/>
                </b>
              </td>
            </tr>

            <xsl:for-each select="clause">
              <xsl:variable name="thisname" select="@name"/>
              <tr class="{@status}">
                <td class="tablebg"/>
                <td>
                  <xsl:value-of select="@name"/>
                </td>
                <td>
                  <xsl:value-of select="@text"/>

                  <xsl:if test="count(test) > 0">
                    <ul class="tests">
                      <xsl:for-each select="test">
                        <li>
                          <xsl:value-of select="."/>
                        </li>
                      </xsl:for-each>
                    </ul>
                  </xsl:if>

                  <xsl:if test="count(issue) > 0">
                    <ul class="issues">
                      <xsl:for-each select="issue">
                        <li>
                          Issue: <xsl:value-of select="."/>
                        </li>
                      </xsl:for-each>
                    </ul>
                  </xsl:if>

                </td>
              </tr>
            </xsl:for-each>

          </xsl:for-each>
        </table>

      </body>
    </html>
  </xsl:template>

</xsl:transform>
