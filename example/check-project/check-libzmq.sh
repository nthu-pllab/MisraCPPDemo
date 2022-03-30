#!/usr/bin/env bash

URL=https://github.com/zeromq/libzmq/archive/v4.3.1.tar.gz
ARCHIVE=v4.3.1.tar.gz
PROJECT=libzmq-4.3.1

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
MISRA_SCAN=$SCRIPT_DIR/../../tools/misra-scan/misra-scan

if [ ! -f $MISRA_SCAN ]; then
    echo "misra-scan not found, specified MISRA_SCAN for $0"
    echo "expected misra-scan path: $MISRA_SCAN"
    exit 1
fi

set -x

wget $URL
tar xzf $ARCHIVE

cd $PROJECT

./autogen.sh
./configure
make clean
$MISRA_SCAN \
    -config-path ../config.json \
    -o ../report \
    make -j`nproc`
