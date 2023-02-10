rm server client
g++ server.cpp socket_tools.cpp -o server -pthread
g++ client.cpp socket_tools.cpp -o client -pthread.sh