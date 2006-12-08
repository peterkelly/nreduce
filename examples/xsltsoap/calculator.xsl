<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://calc"
  xmlns:calc="http://calc"
  version="2.0">

  <xsl:output method="xml"/>

  <xsl:template match="calc:add">
    <addResponse>
      <addReturn>
        <xsl:value-of select="calc:in0 + calc:in1"/>
      </addReturn>
    </addResponse>
  </xsl:template>

  <xsl:template match="calc:multiply">
    <multiplyResponse>
      <multiplyReturn>
        <xsl:value-of select="calc:in0 * calc:in1"/>
      </multiplyReturn>
    </multiplyResponse>
  </xsl:template>

  <xsl:template match="calc:range">
    <rangeResponse>
      <xsl:for-each select="calc:in0 to calc:in1">
        <rangeReturn><xsl:value-of select="."/></rangeReturn>
      </xsl:for-each>
    </rangeResponse>
  </xsl:template>

  <xsl:template match="/">
    <soapenv:Envelope
      xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
      xmlns:xsd="http://www.w3.org/2001/XMLSchema"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xmlns:calc="http://calc">

      <soapenv:Body>
        <xsl:apply-templates select="/soapenv:Envelope/soapenv:Body"/>
      </soapenv:Body>
    </soapenv:Envelope>
  </xsl:template>

</xsl:stylesheet>
