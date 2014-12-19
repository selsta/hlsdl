HLS-Downloader
==============

This program converts .m3u8 playlists to a .ts video. It supports AES-128 encryption.

Requirements
------------

This program needs openssl to decrypt files.

Build
-----

Use `gcc -O2 ./*.c -lcurl`

Windows will follow soon.

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

Todo
----

- Multithreading (Only reason I've written this program)
- Support for SAMPLE-AES and other encryption types.
- openssl in program instead of sytemcall
- Windows
- Remuxing to other formats
