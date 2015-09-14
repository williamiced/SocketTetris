
all:
	gcc Server.c -o Server -pthread -lutil 
	gcc Client.c -o Client -pthread

clean:
	rm -f Server
	rm -f Client
	rm -f initial_screen_dynamic
