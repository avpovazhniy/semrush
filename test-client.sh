#!/bin/bash
PORT=12345
SERVER=localhost
DIR=/usr/include/
cp client $DIR
cd $DIR

for FILE in $(find *.h -maxdepth 1 -type f -exec basename {} \; 2> /dev/null); do $(nohup ./client $FILE $SERVER $PORT >&1 2> /dev/null & ); done
rm client