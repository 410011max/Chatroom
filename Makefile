all: server client
server: server.cpp
		g++ server.cpp -lpthread -o server
client: client.cpp
		g++ client.cpp -lpthread -o client
clean:
	rm -f user_list.csv client server