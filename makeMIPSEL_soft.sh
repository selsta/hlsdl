#!/bin/sh

make clean
BASE_PATH="/mnt/new2/softFPU/openpli/build/tmp/sysroots/"
export TOOLCHAIN_NAME="mipsel-oe-linux"
export PATH=$BASE_PATH"i686-linux/usr/bin/mipsel-oe-linux/":$PATH
export SYSROOT=$BASE_PATH"et4x00"
export CFLAGS="  -mel -mabi=32 -msoft-float -march=mips32 "
export CROSS_COMPILE=$TOOLCHAIN_NAME"-"
CFLAGS="$CFLAGS -pipe -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE "
LDFLAGS=" -L/mnt/new2/curl/out/mipsel_soft/lib -lz -lssl -lcrypto"

if [ $WITH_FFMPEG -eq 1 ];
then
FFMPEG_PATH="/mnt/new2/_BRCM_/exteplayer3/tmp/ffmpeg/tmp/mipsel/ffmpeg-3.2.2/"
CFLAGS+=" -I$FFMPEG_PATH/usr/include/ -L$FFMPEG_PATH/usr/lib/ -DWITH_FFMPEG "
LDFLAGS+=" -lavformat -lavutil -lavcodec -lswresample "
fi

CC="${CROSS_COMPILE}gcc --sysroot=$SYSROOT -fdata-sections -ffunction-sections -Wl,--gc-sections -Os $CFLAGS --sysroot=$SYSROOT $LDFLAGS $CPPFLAGS " make

"$CROSS_COMPILE"strip -s hlsdl
