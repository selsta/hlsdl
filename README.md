HLS-Downloader
==============

This program converts .m3u8 playlists to a .ts video. It supports decryption of both AES-128 and SAMPLE-AES encryption.

Requirements
------------

This program needs openssl to decrypt files.

Build
-----

Use `gcc -O2 ./*.c -lcurl -lavformat -lavutil -lavcodec`

Windows should work already, but is untested. 

Usage and Options
-------
`./hls-download url [options]`

---------------------------

`--best`    or `-b` ... Automaticly choose the best quality.

`--verbose` or `-v` ... Verbose more information.

`--output`  or `-o` ... Choose name of output file.

`--help`    or `-h` ... Print help.

`--force`   or `-f` ... Force overwriting the output file.

`--quiet`   or `-q` ... Print less to the console.

`--dump-dec-cmd`    ... Print the Key and IV of each media segment.

`--dump-ts-urls`    ... Print the links to the .ts files.

Todo
----

- Multithreading
- Remuxing to other formats
