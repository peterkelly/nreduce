#!/bin/bash

. vars

SOURCEFILES=`find .. -name '*.c' -o -name '*.cpp' -o -name '*.txt' | grep -v devtrack | grep -v grammar`

echo '<?xml version="1.0"?>' > output/sourcefiles.xml
echo '<sourcefiles xmlns="http://gridxslt.sourceforge.net/devtrack">' >> output/sourcefiles.xml
for i in $SOURCEFILES; do
  # skip grammar.tab.c; saxon complains about invalid characters in the file
  if [ $i != "../xslt/grammar.tab.c" ]; then
    echo '  <file name="'$i'"/>' >> output/sourcefiles.xml
  fi
done
echo '</sourcefiles>' >> output/sourcefiles.xml

echo '<?xml version="1.0"?>' > output/speclist.xml
echo '<speclist xmlns="http://gridxslt.sourceforge.net/devtrack">' >> output/speclist.xml
for i in $SPECS; do
  echo '  <spec name="'$i'"/>' >> output/speclist.xml
done
echo '</speclist>' >> output/speclist.xml

$XSLTCMD output/sourcefiles.xml examinesource.xsl > output/implementation.xml

$XSLTCMD output/speclist.xml mergeimpl.xsl implfile=output/implementation.xml

for i in $SPECS; do
  $XSLTCMD output/$i-merged.xml genstatus.xsl > output/$i-status.html
done

$XSLTCMD output/speclist.xml genindex.xsl

cp -f style.css output
cp -f script.js output
