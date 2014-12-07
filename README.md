HLS-Downloader
==============

This program converts .m3u8 playlists to .ts files.

Requirements
------------

This program needs openssl to decrypt files and ffmpeg to merge the segments.

Build
-----

Use `gcc -O2 ./*.c -lcurl`

Windows will follow soon.

Todo
----

- Multithreading (Only reason I've written this program)
- Support more kinds of encryption
- Verbose and error log system
- Use of a tmpfolder
- openssl and libavformat in program instead of sytemcall
