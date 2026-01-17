hlsdl
=====

This program converts .m3u8 playlists (using fragmented MPEG-2 Transport Streams) to a .ts video. It supports decryption of both AES-128 and SAMPLE-AES encryption.

Quick Start
-----------

## Build

Requirements
------------

### Common:
- libcurl
- OpenSSL/libcrypto

### Windows

#### Prerequisites

- Visual Studio 2017 or newer
- CMake
- Git
- Pre-compiled OpenSSL for MSVC
- pthreads-win32

See detailed Windows build instructions in [BUILD_WINDOWS.txt](msvc/BUILD_WINDOWS.txt).

### macOS

#### Prerequisites

- Xcode Command Line Tools
- Homebrew (recommended for OpenSSL)

```bash
# If you get OpenSSL errors:
brew install openssl
```

### macOS/Linux:

Clone the repository

```bash
git clone https://github.com/selsta/hlsdl
cd hlsdl
```

# Compile

```bash
# Install (optional, copies to /usr/local/bin)
sudo make install
```

# Run

```
./hlsdl [url]
```

**Note:** 

If you encounter `xcrun` errors during `make`, try:

```bash
xcode-select --install
# If already installed, run:
sudo xcode-select --reset
```

Or `"openssl/conf.h file not found"` ?

```bash
brew install openssl
```

### Linux

#### Prerequisites

```bash
# Ubuntu/Debian:
sudo apt-get install libcurl4-openssl-dev libssl-dev build-essential

# Fedora/RHEL:
sudo dnf install libcurl-devel openssl-devel gcc make
```

Compile:
```bash
make && sudo make install && make clean
```

### Docker
```bash
docker build -t hlsdl:latest .
docker run -v ./data:/var/hlsdl/data --rm -it hlsdl:latest hlsdl [options] url
```

Troubleshooting
---------------

### Program not found after installation
If `hlsdl` command isn't recognized, either:
1. Run from the build directory: `./hlsdl [url]`
2. Install with `sudo make install` (places in `/usr/local/bin`)
3. Add the installation directory to your PATH

Usage and Options
-----------------
`./hlsdl [options] url`

```
docker run -v ./data:/var/hlsdl/data --rm -it hlsdl:latest hlsdl [options] url
```

## Basic Usage Examples
```bash
# Download a stream
./hlsdl -o output.ts https://example.com/stream.m3u8

# Choose best quality
./hlsdl -b https://example.com/stream.m3u8

# Set output filename
./hlsdl -o my_video.ts https://example.com/stream.m3u8

# Verbose output
./hlsdl -v https://example.com/stream.m3u8
```

## All Options
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

FAQ
---

### Q: Does hlsdl support DRM-protected streams?
A: hlsdl supports AES-128 and SAMPLE-AES encryption but does **not** support DRM-protected streams (like Widevine, PlayReady, FairPlay, etc.). DRM-protected content requires proprietary decryption modules that cannot be used with hlsdl.

### Q: How do I find the .m3u8 URL from a website?
A: You can use browser developer tools (F12):
1. Open Network tab
2. Filter by "m3u8" or "media"
3. Play the video
4. Look for .m3u8 requests in the network log

### Q: Can I download streams that play in my browser?
A: Only if they use standard AES-128 encryption without DRM. Many streaming services use DRM which prevents downloading.

### Q: What about SAMPLE-AES with DRM?
A: Not supported. SAMPLE-AES without DRM is supported, but when combined with DRM (like FairPlay), it cannot be decrypted with hlsdl.

### Q: The download stops at "DRM detected"
A: This means the stream uses DRM protection that hlsdl cannot decrypt. The `-F` option can force ignore this detection, but the resulting file will be unplayable.

ToDo
-----
* support for Fragmented MPEG-4 playlist
* support for EXT-X-MAP in the MPEG-2 Transport Streams playlist

Ideas
-----

- Multithreading

License
-------

[MIT License](https://github.com/selsta/hlsdl/blob/master/LICENSE)
