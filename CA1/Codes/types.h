#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <vector>
#include <string>
#include <map>
#include <signal.h>
#include <chrono>
#include <thread>
#include <sstream>

#include <string>
using namespace std;

const int BACKLOG = 5;
const int BUFFER_SIZE = 1024;
const int BROADCAST_PORT = 9090;
const int NUM_CLIENT_INPUTS = 3;
const int NUM_SERVER_INPUTS = 4;
const int TIME_OUT = 10;
const int FAILED = -1;
const int ROOM_NOT_FOUND = -1;
int None = -1;
const int PLAYER_1 = 1;
const int PLAYER_2 = 2;
const int DRAW = 0;
int TIME_OVER = 0;
const int MAX_CLIENTS =2;

const string CONNECTION_ENABLED = "Connection enabeled.";
const string SOCKET_CREATION_FAILED = "Socket creation failed.";
const string INVALID_ADDRESS = "Invalid address or address not suported.";
const string BIND_FAILED = "Bind failed.";
const string LISTEN_FAILED = "Listen failed.";
const string CONNECTION_FAILED = "Connection failed.";
const string CLIENT_CONNECTED = "Client connected.";
const string SERVER_INPUT_ERROR = "Usage: ./server.out {IP} {Port} {Number of rooms}";
const string CLIENT_INPUT_ERROR = "Usage: ./server.out {IP} {Port}";

const string STAR = "*";
const string NO_READ = "---";
const string END_GAME_COMMAND = "end_game";
const string BROADCAST_IP = "127.255.255.255";

const char *RECEIVE_NAME_MESSAGE = "*!!!*Please enter your name:";
const char *RECEIVE_ROOM_NUMBER_MESSAGE = "*@@@*These are available rooms. Please select one of them and enter it's number:";
const char *ROOM_UNAVAILABLE_MESSAGE = "*@@@*This room is not available right now. Please try again.";
const char *FIRST_PLAYER_MESSAGE = "*$$$*Game will start soon! Please wait...";
const char *SECOND_PLAYER_MESSAGE = "*^^^*Let's start!";
const char *CHOOSE_MOVE_MESSAGE = "*&&&*Choose between\n1)rock\n2)paper\n3)scissors";
const char *END_GAME_MESSAGE = "*(((*The game ended. Here is the list of players and win counts:\n";

const string NAME_INP_OPC = "!!!";
const string ROOM_CHOOSE_OPC = "@@@";
const string CONNECT_WITH_ROOM_OPC = ")))";
const string PLAYER1_OPC = "$$$";
const string PLAYER2_OPC = "^^^";
const string GAME_BEGIN_OPC = "&&&";
const string ENCODED_CHOICE_OPC = "===";
const string JUDGE_RESULT_OPC = "+++";
const string FINAL_MSG_OPC = "(((";

enum class Move {

    NONE = 0,
    ROCK = 1,
    PAPER = 2,
    SCISSORS = 3
};

struct broadcast_info
{
    int sock;
    struct sockaddr_in bc_address;
};

struct Room
{
    int room_socket;
    int room_number;
    int player_1;
    int player_2;
    vector<int> players;
    bool asked_player1_choice = false;
    bool asked_player2_choice = false;
    int p1_choice = None;
    int p2_choice = None;
    int winner = None;
};


#endif