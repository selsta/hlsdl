#!/bin/bash

set -e

export CURL_TYPE="OPENSSL"

MYTOPDIR=$PWD
S_SRC="$MYTOPDIR/src/main.c $MYTOPDIR/src/aes.c $MYTOPDIR/src/curl.c $MYTOPDIR/src/hls.c $MYTOPDIR/src/misc.c $MYTOPDIR/src/msg.c $MYTOPDIR/src/mpegts.c"

cd /mnt/new2/sysroots

source supportedPlatforms.sh

EPLATFORM_MAIN="${EPLATFORM%%_*}"
if [ "$EPLATFORM_MAIN" != "$EPLATFORM" ]
then
    EPLATFORM_VARIANT="_${EPLATFORM#*_}"
    #EPLATFORM_VARIANT="_${EPLATFORM##*_}"
else
    EPLATFORM_VARIANT=""
fi

OUTDIR=$MYTOPDIR/out/$EPLATFORM_MAIN
mkdir -p $OUTDIR
BINARY_NAME="hlsdl${EPLATFORM_VARIANT}_static_curl_openssl.${OPENSSL_VER}"

rm -rf $OUTDIR/$BINARY_NAME
mkdir -p $OUTDIR

if [ "${TOOLCHAIN_NAME}" != "" ];
then
    export CC="${TOOLCHAIN_NAME}-${CC}"
    STRIP="${TOOLCHAIN_NAME}-strip"
else
    export CC="${CC}"
    STRIP="strip"
fi

#
$CC -O2 -fdata-sections -ffunction-sections -Wl,--gc-sections -D_GNU_SOURCE=1 -std=gnu99 -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare $S_SRC $MYSYSROOT_DIR/curl_openssl/lib/libcurl.a -lrt -lpthread -lz -lssl -lcrypto -o $OUTDIR/$BINARY_NAME
#$CC -O0 -g -fdata-sections -ffunction-sections -Wl,--gc-sections -D_GNU_SOURCE=1 -std=gnu99 -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare $S_SRC $MYSYSROOT_DIR/curl_openssl/lib/libcurl.a -lrt -lpthread -lz -lssl -lcrypto -o $OUTDIR/$BINARY_NAME

$STRIP -s $OUTDIR/$BINARY_NAME
