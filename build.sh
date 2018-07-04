gcc -std=c99 client.c common.c -o client -Wl,--gc-sections,--strip-all
gcc -std=c99 server.c common.c -o server -lpthread -Wl,--gc-sections,--strip-all