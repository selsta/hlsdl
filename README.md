hlsdl
=====

This program downloads VOD and live .m3u8 (HLS) streams to a single file. It handles MPEG-2 Transport Stream and fragmented MP4 / CMAF segments, `EXT-X-MAP` initialization segments (fMP4 and TS), `EXT-X-BYTERANGE`, discontinuities, and decryption of both AES-128 and SAMPLE-AES.

Requirements
------------

This program requires libcurl and libcrypto libraries.

Build
-----

[![hlsdl build](https://github.com/selsta/hlsdl/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/selsta/hlsdl/actions/workflows/build.yml)

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

-R ... Resume an interrupted VOD download (keeps a <output>.hlsdl.resume sidecar).

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

Ideas
-----

- Multithreading

License
-------

[MIT License](https://github.com/selsta/hlsdl/blob/master/LICENSE)
