#!/bin/bash

set -e

for platformIter in "i686" "sh4" "sh4_old" "mipsel" "mipsel_old" "mipsel_softfpu" "armv7" "armv5t"; do
    for opensslVerIter in "0.9.8" "1.0.0" "1.0.2"; do
        ./make.sh    $platformIter $opensslVerIter
    done
done

