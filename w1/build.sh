rm server client
g++ server.cpp socket_tools.cpp -o server
g++ client.cpp socket_tools.cpp thread_functions.cpp -o client
