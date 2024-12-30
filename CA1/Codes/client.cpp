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
#include <iostream>

#include "logger.h"
#include "types.h"

int setupSocket()
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        logError(SOCKET_CREATION_FAILED.c_str());
        exit(EXIT_FAILURE);
    }
    return server_socket;
}

struct sockaddr_in defineAddress(int port, const char *ip)
{
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip);
    return server_address;
}

void connect(int &server_fd, struct sockaddr_in &server_address)
{
    if (connect(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        logMsg(CONNECTION_FAILED.c_str());
        exit(EXIT_FAILURE);
    }
}

int connectServer(int port, const char *ip)
{
    int server_socket = setupSocket();
    struct sockaddr_in server_address = defineAddress(port, ip);
    connect(server_socket, server_address);
    return server_socket;
}

void bindSocket(int socket, struct sockaddr_in bc_address)
{
    if (bind(socket, (struct sockaddr *)&bc_address, sizeof(bc_address)) < 0)
    {
        logError(BIND_FAILED.c_str());
        exit(EXIT_FAILURE);
    }
}

int setBroadcastSocket()
{
    int broadcast = 1, opt = 1;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    return sock;
}

int connectBroadcastSocket()
{
    int socket = setBroadcastSocket();
    struct sockaddr_in bc_address = defineAddress(BROADCAST_PORT, BROADCAST_IP.c_str());
    bind(socket, (struct sockaddr *)&bc_address, sizeof(bc_address));

    struct broadcast_info bc_inf;
    bc_inf.sock = socket;
    bc_inf.bc_address = bc_address;

    return socket;
}

void intiFileDescriptorSet(int &max_sd, fd_set &master_set, int &server_socket, int &broadcast_socket)
{
    FD_ZERO(&master_set);
    max_sd = server_socket;
    FD_SET(server_socket, &master_set);

    if (broadcast_socket > max_sd)
        max_sd = broadcast_socket;
    FD_SET(broadcast_socket, &master_set);

    if (STDIN_FILENO > max_sd)
        max_sd = STDIN_FILENO;
    FD_SET(STDIN_FILENO, &master_set);

    if (STDOUT_FILENO > max_sd)
        max_sd = STDOUT_FILENO;
    FD_SET(STDOUT_FILENO, &master_set);
}

pair<string, string> extractPatternAndMessage(const std::string &str)
{
    pair<string, string> info;
    size_t first_star = str.find(STAR.c_str());
    size_t second_star = str.find(STAR.c_str(), first_star + 1);

    info.first = str.substr(first_star + 1, second_star - first_star - 1);
    info.second = str.substr(second_star + 1);

    return info;
}

void alarmHandler(int sig)
{
    TIME_OVER = 1;
}

string encodedMoveChoice(int room_number, int player_number, char *buffer)
{
    int move = atoi(buffer);
    string encoded_message = STAR + ENCODED_CHOICE_OPC + STAR;
    encoded_message += to_string(room_number) + STAR + to_string(player_number) +
                       STAR + to_string(move);
    return encoded_message;
}

void signalHandler(int signal)
{
    exit(EXIT_SUCCESS);
}

void readAndSendBack(char *buffer, string &read_state, int &server_socket)
{
    read(STDIN_FILENO, buffer, 1024);
    string buf_str(buffer);

    buf_str = STAR.c_str() + read_state + STAR.c_str() + buf_str;
    send(server_socket, buf_str.c_str(), strlen(buf_str.c_str()), 0);
    read_state = NO_READ;
}

int main(int argc, char const *argv[])
{
    if (argc != NUM_CLIENT_INPUTS)
    {
        logError(CLIENT_CONNECTED.c_str());
        exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int server_socket = connectServer(port, ip);
    int broadcast_socket = connectBroadcastSocket();
    logInfo(CLIENT_CONNECTED.c_str());

    int max_sd;
    fd_set master_set, working_set;
    intiFileDescriptorSet(max_sd, master_set, server_socket, broadcast_socket);

    signal(SIGINT, signalHandler);

    int room_connected, player_number, room_number;

    string read_state = NO_READ;
    char buff[BUFFER_SIZE] = {0};

    while (1)
    {
        working_set = master_set;
        select(max_sd + 1, &working_set, NULL, NULL, NULL);

        for (int i = 0; i <= max_sd; i++)
        {
            if (FD_ISSET(i, &working_set))
            {
                if (i == STDIN_FILENO && read_state != NO_READ)
                    readAndSendBack(buff, read_state, server_socket);
                else
                    recv(i, buff, BUFFER_SIZE, 0);

                string new_message(buff), pattern, message;

                pair<string, string> info = extractPatternAndMessage(new_message);
                pattern = info.first;
                message = info.second;

                if (pattern == NAME_INP_OPC)
                {
                    logMsg(message.c_str());
                    memset(buff, 0, BUFFER_SIZE);
                    read_state = NAME_INP_OPC;
                }
                if (pattern == ROOM_CHOOSE_OPC)
                {
                    logMsg(message.c_str());
                    memset(buff, 0, BUFFER_SIZE);
                    read_state = ROOM_CHOOSE_OPC;
                }
                else if (pattern == CONNECT_WITH_ROOM_OPC)
                {
                    int port_num = stoi(message);
                    room_connected = connectServer(port_num, ip);
                    room_number = port_num - port;

                    if (room_connected > max_sd)
                        max_sd = room_connected;
                    FD_SET(room_connected, &master_set);
                }
                else if (pattern == PLAYER1_OPC)
                {
                    logMsg(message.c_str());
                    player_number = PLAYER_1;
                }
                else if (pattern == PLAYER2_OPC)
                {
                    logMsg(message.c_str());
                    player_number = PLAYER_2;
                }
                else if (pattern == GAME_BEGIN_OPC)
                {
                    logMsg(message.c_str());
                    signal(SIGALRM, alarmHandler);
                    siginterrupt(SIGALRM, 1);

                    alarm(TIME_OUT);
                    memset(buff, 0, BUFFER_SIZE);
                    read(STDIN_FILENO, buff, BUFFER_SIZE);
                    while (true)
                        if (TIME_OVER)
                        {
                            if (player_number == PLAYER_2)
                                this_thread::sleep_for(chrono::milliseconds(TIME_OUT));
                            string message = encodedMoveChoice(room_number, player_number, buff);
                            send(room_connected, message.c_str(), strlen(message.c_str()), 0);
                            TIME_OVER = false;
                            break;
                        }
                }
                else if (pattern == JUDGE_RESULT_OPC)
                    write(STDOUT_FILENO, message.c_str(), strlen(message.c_str()));
                else if (pattern == FINAL_MSG_OPC)
                {
                    logMsg(message.c_str());
                    exit(EXIT_SUCCESS);
                }

                memset(buff, 0, BUFFER_SIZE);
            }
        }
    }

    return 0;
}