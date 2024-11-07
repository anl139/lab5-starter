all: servers
  
servers: number-server.c
	gcc -std=c11 -Wall -Wno-unused-variable -fsanitize=address -g http-server.c number-server.c -o servers

clean:
	rm -f servers

