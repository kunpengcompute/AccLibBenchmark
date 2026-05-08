#! /bin/sh
cd `dirname $0`
source config.sh
if [ "$PBLIB_HOME" == "" ]
then
    echo "environment PBLIB_HOME must exist!"
    exit 1
fi
echo "Current protobuf library home: $PBLIB_HOME"
