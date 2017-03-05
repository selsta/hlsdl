#!/bin/bash

set -e

MYTOPDIR=$PWD
S_SRC="$MYTOPDIR/src/main.c $MYTOPDIR/src/aes.c $MYTOPDIR/src/curl.c $MYTOPDIR/src/hls.c $MYTOPDIR/src/misc.c $MYTOPDIR/src/msg.c"

cd /mnt/new2/sysroots

source supportedPlatforms.sh

OUTDIR=$MYTOPDIR/out
rm -rf $OUTDIR/hlsdl_${EPLATFORM}_static_curl_openssl.${OPENSSL_VER}
mkdir -p $OUTDIR

if [ "${TOOLCHAIN_NAME}" != "" ];
then
    export CC="${TOOLCHAIN_NAME}-${CC}"
    STRIP="${TOOLCHAIN_NAME}-strip"
else
    export CC="${CC}"
    STRIP="strip"
fi

$CC -fdata-sections -ffunction-sections -Wl,--gc-sections -D_GNU_SOURCE=1 -std=gnu99 -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare $S_SRC $MYSYSROOT_DIR/lib/libcurl.a -lrt -lpthread -lz -lssl -lcrypto -o $OUTDIR/hlsdl_${EPLATFORM}_static_curl_openssl.${OPENSSL_VER}
$STRIP -s $OUTDIR/hlsdl_${EPLATFORM}_static_curl_openssl.${OPENSSL_VER}
