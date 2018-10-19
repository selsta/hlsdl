hlsdl
==============

This program converts .m3u8 playlists to a .ts video. It supports decryption of both AES-128 and SAMPLE-AES encryption.

Requirements
------------

This program requires FFmpeg installed in order to decrypt SAMPLE-AES content.

Build
-----

Use `make && make install && make clean` to install.

Usage and Options
-------
`./hlsdl url [options]`

---------------------------
```
-b ... Automaticly choose the best quality.

-v ... Verbose more information.

-o ... Choose name of output file.

-u ... Set custom HTTP User-Agent header.

-h ... Set custom HTTP header.

-p ... Set proxy uri.

-k ... Allow to replace part of AES key uri - old.

-n ... Allow to replace part of AES key uri - new.

-f ... Force overwriting the output file.

-q ... Print less to the console.

-d ... Print the openssl decryption command.

-t ... Print the links to the .ts files.

-s ... Set live start offset in seconds.

-e ... Set refresh delay in seconds.

-r ... Set max retries at open.

-w ... Set max download segment retries.

-a ... Set additional url to the audio media playlist.
```
Todo
----

- removing ffmpeg dependency and use own simple mpegts demux implementation

