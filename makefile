all: server client
client: client.cpp
	g++ -o client client.cpp -pthread
server: server.cpp
	g++ -o server server.cpp -pthread