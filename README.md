hlsdl
=====

This program downloads .m3u8 playlists containing MPEG-2 Transport Streams or fragmented MP4. It supports AES-128 decryption and SAMPLE-AES decryption for MPEG-2 Transport Streams.

Requirements
------------

This program requires libcurl and libcrypto libraries.

Build
-----

![hlsdl build](https://github.com/selsta/hlsdl/workflows/hlsdl%20build/badge.svg)

Linux:
`make && make install && make clean`

Windows:
https://github.com/selsta/hlsdl/blob/master/msvc/BUID_WINDOWS.txt

Docker: `docker build -t hlsdl:latest .`


Usage and Options
-----------------
`./hlsdl [options] url`

```
docker run -v ./data:/var/hlsdl/data --rm -it hlsdl:latest hlsdl [options] url
```

Output keeps the original container format; use `-o video.mp4` for fragmented MP4.
Fragmented MP4 discontinuities and changes to initialization data are not supported.
Separate audio and SAMPLE-AES are unsupported for playlists using `EXT-X-MAP`.

---------------------------
```
-b ... Automatically choose the best quality.

-W ... Choose largest width lower or equal than this.

-H ... Choose largest height lower or equal than this.

-A ... Select audio language.

-v ... Verbose more information.

-o ... Choose name of output file ("-" alias for stdout).

-u ... Set custom HTTP User-Agent header.

-h ... Set custom HTTP header.

-p ... Set proxy uri.

-k ... Allow to replace part of AES key uri - old.

-n ... Allow to replace part of AES key uri - new.

-f ... Force overwriting the output file.

-F ... Force ignore detection of DRM.

-K ... Force AES key value (hexstring)

-q ... Print less to the console.

-d ... Print the openssl decryption command.

-t ... Print the links to the .ts files.

-s ... Set live start offset in seconds.

-i ... Set live stream download duration in seconds.

-e ... Set refresh delay in seconds.

-r ... Set max retries at open.

-w ... Set max download segment retries.

-a ... Set additional url to the audio media playlist.

-c ... Treat HTTP code 206 as 200 even if request was made without range header.

-C ... the file name of file holding cookie data in the old Netscape / Mozilla cookie data format.
```

ToDo
-----
* remux fragmented MP4 discontinuities and separate audio renditions

Ideas
-----

- Multithreading

License
-------

[MIT License](https://github.com/selsta/hlsdl/blob/master/LICENSE)
