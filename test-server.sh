#!/bin/bash
UPLOAD=./upload
mkdir $UPLOAD
PORT=12345
cp ./server $UPLOAD
cd $UPLOAD
./server $PORT
