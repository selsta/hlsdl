all:
	gcc -O2 main.c aes.c curl.c hls.c misc.c msg.c -o hlsdl -lcurl -lavformat -lavutil -lavcodec
