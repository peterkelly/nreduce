#!/bin/bash

. vars

if [ ! -d output ]; then
  mkdir output
fi
rm -f output/*

cp specs/*.dtd output
cp specs/*.ent output

for i in xslt20 xpath20 xpath-functions xslt-xquery-serialization; do
  tidy -asxhtml -i2 specs/$i.html > output/$i-source.xml 2>/dev/null
  cat output/$i-source.xml | perl -pe 's/\"http:\/\/www\.w3\.org\/TR\/xhtml1\/DTD\//\"/' > \
      output/$i-source1.xml
  $XSLTCMD output/$i-source1.xml examinespec.xsl > output/$i-details.xml
done

$XSLTCMD specs/xmlschema-structures.xml examinestructures.xsl > output/xmlschema-1-details.xml

./update.sh
