#!/bin/bash

echo === webupdate === Creating archive
tar -jcvf temp.tar.bz2 output/*.html output/*.js output/*.css
echo === webupdate === Uploading to sourceforge
scp temp.tar.bz2 pmkelly@shell.sourceforge.net:/home/groups/g/gr/gridxslt/htdocs
echo === webupdate === Uncompressing archive and replacing existing devtrack files
ssh pmkelly@shell.sourceforge.net 'cd /home/groups/g/gr/gridxslt/htdocs; tar -jxvf temp.tar.bz2; rm -rf devtrack; mv output devtrack'
echo === webupdate === Removing temporary files
rm -f temp.tar.bz2
echo === webupdate === Done
