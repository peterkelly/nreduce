<?xml version="1.0"?>
<xsl:transform
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  xmlns:fn="http://www.w3.org/2005/04/xpath-functions"
  xmlns:f="http://my.functions"
  xmlns:dt="http://gridxslt.sourceforge.net/devtrack"
  xmlns="http://www.w3.org/1999/xhtml"
  exclude-result-prefixes="xsl xs fn f dt"
  version="2.0">

  <xsl:include href="util.xsl"/>

  <xsl:function name="f:countimplreq" as="xs:integer">
    <xsl:param name="s"/>
    <xsl:value-of select="count($s//dt:clause[@status != 'info'])"/>
  </xsl:function>

  <xsl:function name="f:countprotoimplreq" as="xs:integer">
    <xsl:param name="s"/>
    <xsl:value-of select="count($s//dt:clause[@status != 'info' and @status != 'deferred'])"/>
  </xsl:function>

  <xsl:template match="/">
    <xsl:variable name="tasks" select="/dt:spec"/>

    <xsl:variable name="speclist" select="/dt:speclist"/>

    <xsl:result-document href="output/top.html">
      <html>
        <head>
          <title>Development tracking</title>
          <link rel="stylesheet" type="text/css" href="style.css"/>
          <style type="text/css">
            html { background: #F0F0F0; }
          </style>
          <script language="javascript" src="script.js"/>
        </head>
        <body>

          <table width="100%" border="0" cellspacing="0" cellpadding="0">
            <tr>
              <td align="left">
                Specifications:
                <xsl:for-each select="$speclist/dt:spec">
                  <a href="{concat(@name,'-status.html')}" target="main">
                    <xsl:value-of select="@name"/>
                  </a>
                  &#160;
                </xsl:for-each>
              </td>
              <td align="right">
                <a href="main.html" target="main">Main page</a>
                &#160;
                <a href="http://gridxslt.sourceforge.net/Development_tracking"
                  target="_top">What is this?</a>
              </td>
            </tr>
          </table>

          <form name="options">

            <table width="100%" border="0" cellspacing="0" cellpadding="0">
              <tr>
                <td align="left">
                  Show clasues that are: 
                  <span class="supported">
                    <input type="checkbox" onclick="update()" checked="1"
                      name="supported" id="supported"/>Supported
                  </span>
                  <span class="unsupported">
                    <input type="checkbox" onclick="update()" checked="1" 
                      name="unsupported" id="unsupported"/>Unsupported
                  </span>
                  <span class="partial">
                    <input type="checkbox" onclick="update()" checked="1" 
                      name="partial" id="partial"/>Partial
                  </span>
                  <span class="deferred">
                    <input type="checkbox" onclick="update()" checked="1" 
                      name="deferred" id="deferred"/>Deferred
                  </span>
                  <span class="info">
                    <input type="checkbox" onclick="update()" checked="1" 
                      name="info" id="info"/>Info
                  </span>
                </td>
                <td align="right">
                  <input type="checkbox" onclick="update()" 
                    name="tests" id="tests"/>Show testcases
                  <input type="checkbox" onclick="update()" 
                    name="issues" id="issues"/>Show issues
                </td>
              </tr>
            </table>

          </form>
        </body>
      </html>
    </xsl:result-document>

    <xsl:result-document href="output/index.html">
      <html>

        <head>
          <title>Development tracking</title>
        </head>

        <frameset rows="80,*">
          <frame name="top" src="top.html"/>
          <frame name="main" src="main.html"/>
        </frameset>

      </html>
    </xsl:result-document>

    <xsl:variable name="specs"
      select="for $name in $speclist/dt:spec/@name
                return document(concat('output/',$name,'-merged.xml'))/dt:spec"/>

    <xsl:variable name="stats1">
      <xsl:for-each select="$stati">
        <xsl:variable name="status" select="."/>
        <dt:status name="{.}">
          <xsl:for-each select="$specs">
            <dt:spec name="{@name}" count="{count(dt:section/dt:clause[@status=$status])}"/>
          </xsl:for-each>
          <dt:total count="{count($specs/dt:section/dt:clause[@status=$status])}"/>
        </dt:status>
      </xsl:for-each>
    </xsl:variable>

    <xsl:variable name="stats">
      <xsl:copy-of select="$stats1/node()"/>
      <dt:totals>
        <xsl:for-each select="$specs">
          <xsl:variable name="specname" select="@name"/>
          <dt:spec name="{@name}"
            all="{sum($stats1/dt:status/dt:spec[@name=$specname]/@count)}"
            proto="{sum($stats1/dt:status[@name != 'info' and @name != 'deferred']/
                   dt:spec[@name=$specname]/@count)}"
            full="{sum($stats1/dt:status[@name != 'info']/dt:spec[@name=$specname]/@count)}"/>

        </xsl:for-each>
        <dt:allspecs
          all="{sum($stats1/dt:status/dt:spec/@count)}"
          proto="{sum($stats1/dt:status[@name != 'info' and @name != 'deferred']/dt:spec/@count)}"
          full="{sum($stats1/dt:status[@name != 'info']/dt:spec/@count)}"/>
      </dt:totals>
    </xsl:variable>

    <xsl:result-document href="output/main.html">
      <html>

        <head>
          <title>Development tracking</title>
          <link rel="stylesheet" type="text/css" href="style.css"/>
        </head>

        <body>

          <h2>Specifications</h2>

          <table>
            <tr>
              <th>Title</th>
              <th>Name</th>
              <th>Progress towards prototype</th>
              <th>Progress towards full implementation</th>
            </tr>
            <xsl:for-each select="$specs">
              <xsl:variable name="specname" select="@name"/>
              <tr>
                <td>
                  <a href="{@url}" target="_top"><xsl:value-of select="@title"/></a>
                  [<a href="{concat(@name,'-status.html')}">details</a>]
                </td>
                <td><xsl:value-of select="@name"/></td>
                <td align="center">
                  <xsl:variable name="supported"
                    select="$stats/dt:status[@name='supported']/dt:spec[@name=$specname]/@count"/>
                  <xsl:variable name="total"
                    select="$stats/dt:totals/dt:spec[@name=$specname]/@proto"/>
                  <xsl:value-of select="round($supported*100 div $total)"/>%
                </td>
                <td align="center">
                  <xsl:variable name="supported"
                    select="$stats/dt:status[@name='supported']/dt:spec[@name=$specname]/@count"/>
                  <xsl:variable name="total"
                    select="$stats/dt:totals/dt:spec[@name=$specname]/@full"/>
                  <xsl:value-of select="round($supported*100 div $total)"/>%
                </td>
              </tr>
            </xsl:for-each>
            <tr>
              <td colspan="4">&#160;</td>
            </tr>
            <tr style="font-weight: bold; color: red">
              <td>Overall progress</td>
              <td></td>
              <td align="center">
                <xsl:variable name="supported"
                  select="$stats/dt:status[@name='supported']/dt:total/@count"/>
                <xsl:variable name="total"
                  select="$stats/dt:totals/dt:allspecs/@proto"/>
                <xsl:value-of select="round($supported*100 div $total)"/>%
              </td>
              <td align="center">
                <xsl:variable name="supported"
                  select="$stats/dt:status[@name='supported']/dt:total/@count"/>
                <xsl:variable name="total"
                  select="$stats/dt:totals/dt:allspecs/@full"/>
                <xsl:value-of select="round($supported*100 div $total)"/>%
              </td>
            </tr>
          </table>

          <h2>Statistics</h2>

          <table>
            <tr>
              <th>Statistic</th>
              <xsl:for-each select="$specs">
                <th>
                  <xsl:value-of select="@name"/>
                </th>
              </xsl:for-each>
              <th>Total</th>
            </tr>

            <tr>
              <td>Sections</td>
              <xsl:for-each select="$specs">
                <td>
                  <xsl:value-of select="count(dt:section)"/>
                </td>
              </xsl:for-each>
              <td><xsl:value-of select="count($specs/dt:section)"/></td>
            </tr>

            <tr>
              <td>Clauses</td>
              <xsl:for-each select="$stats/dt:totals/dt:spec">
                <td>
                  <xsl:value-of select="@all"/>
                </td>
              </xsl:for-each>
              <td><xsl:value-of select="$stats/dt:totals/dt:allspecs/@all"/></td>
            </tr>

            <tr>
              <td>Clauses requiring implementation for full conformance</td>
              <xsl:for-each select="$stats/dt:totals/dt:spec">
                <td>
                  <xsl:value-of select="@full"/>
                </td>
              </xsl:for-each>
              <td><xsl:value-of select="$stats/dt:totals/dt:allspecs/@full"/></td>
            </tr>

            <tr>
              <td>Clauses requiring implementation for prototype</td>
              <xsl:for-each select="$stats/dt:totals/dt:spec">
                <td>
                  <xsl:value-of select="@proto"/>
                </td>
              </xsl:for-each>
              <td><xsl:value-of select="$stats/dt:totals/dt:allspecs/@proto"/></td>
            </tr>

            <xsl:for-each select="$stats/dt:status">
              <tr>
                <td class="{concat(@name,'const')}">
                  <xsl:value-of select="@name"/>
                </td>
                <xsl:for-each select="dt:spec">
                  <xsl:variable name="specname" select="@name"/>
                  <xsl:variable name="spectotalclauses"
                    select="$stats/dt:totals/dt:spec[@name=$specname]/@all"/>
                  <td>
                    <xsl:value-of select="@count"/>
                    (<xsl:value-of select="round(@count*100 div $spectotalclauses)"/>%)
                  </td>
                </xsl:for-each>
                <td>
                  <xsl:value-of select="dt:total/@count"/>
                </td>
              </tr>
            </xsl:for-each>



          </table>

          Page generated <xsl:value-of select="current-dateTime()"/>

        </body>

      </html>
    </xsl:result-document>

  </xsl:template>

</xsl:transform>
