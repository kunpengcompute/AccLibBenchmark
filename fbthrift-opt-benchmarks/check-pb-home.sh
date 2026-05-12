#! /bin/sh
cd `dirname $0`
source config.sh
if [ "$FBTHRIFT_HOME" == "" ]
then
    echo "environment FBTHRIFT_HOME must exist!"
    exit 1
fi
echo "Current fbthrift home: $FBTHRIFT_HOME"
