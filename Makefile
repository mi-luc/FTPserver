all: client serv

client:
	gcc client.c -o client
	
serv:
	gcc -pthread -g server.c -o serv
clean:
	rm serv client