CXX = g++

CXXFLAGS = -Wall -g

SERVER_SRC = server.cpp
CLIENT_SRC = client.cpp

LOGGER_SRC = logger.cpp
SOCKET_SRC = socket.cpp

TYPES_HEADER = types.h

SERVER_OUT = server.out
CLIENT_OUT = client.out

all: $(SERVER_OUT) $(CLIENT_OUT)

$(SERVER_OUT): $(SERVER_SRC) $(SERVER_DEPS) $(LOGGER_SRC) $(TYPES_HEADER) 
	$(CXX) $(CXXFLAGS) -o $(SERVER_OUT) $(SERVER_SRC) $(LOGGER_SRC) $(TYPES_HEADER) 

$(CLIENT_OUT): $(CLIENT_SRC) $(CLIENT_DEPS) $(LOGGER_SRC) $(TYPES_HEADER) 
	$(CXX) $(CXXFLAGS) -o $(CLIENT_OUT) $(CLIENT_SRC) $(LOGGER_SRC) $(TYPES_HEADER)

clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)

run: all
	@echo "Running server..."
	./$(SERVER_OUT)
	I@echo "Running client..."
	./$(CLENT_OUT)

.PHONY: all clean run