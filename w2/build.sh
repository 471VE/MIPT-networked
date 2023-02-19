rm client lobby server
g++ client.cpp -lenet -o client
g++ lobby.cpp -lenet -o lobby
g++ server.cpp -lenet -o server