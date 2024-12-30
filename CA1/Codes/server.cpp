#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <thread>
#include <sstream>
#include <signal.h>

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

void bindSocket(int socket, struct sockaddr_in bc_address)
{
    if (bind(socket, (struct sockaddr *)&bc_address, sizeof(bc_address)) < 0)
    {
        logError(BIND_FAILED.c_str());
        exit(EXIT_FAILURE);
    }
}

void startListening(int server_socket)
{
    if (listen(server_socket, BACKLOG) < 0)
    {
        logError(LISTEN_FAILED.c_str());
        exit(EXIT_FAILURE);
    }
}

int setupServer(int port, const char *ip)
{
    int server_socket = setupSocket();
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in server_address = defineAddress(port, ip);
    bindSocket(server_socket, server_address);
    startListening(server_socket);
    return server_socket;
}

int setBroadcastSocket()
{
    int broadcast = 1, opt = 1;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    return sock;
}

struct broadcast_info connectBroadcastSocket()
{
    int socket = setBroadcastSocket();
    struct sockaddr_in bc_address = defineAddress(BROADCAST_PORT, BROADCAST_IP.c_str());
    bind(socket, (struct sockaddr *)&bc_address, sizeof(bc_address));

    struct broadcast_info bc_inf;
    bc_inf.sock = socket;
    bc_inf.bc_address = bc_address;

    return bc_inf;
}

void terminateGame(char *buffer, map<string, int> players_win_count, struct broadcast_info bc_address)
{
    read(STDIN_FILENO, buffer, BUFFER_SIZE);
    string buf_str(buffer);
    buf_str.pop_back();

    if (buf_str == END_GAME_COMMAND)
    {
        string final_msg = END_GAME_MESSAGE;
        for (const auto &pair : players_win_count)
        {
            final_msg = final_msg + pair.first + ": " + to_string(pair.second) + "\n";
        }
        sendto(bc_address.sock, final_msg.c_str(), strlen(final_msg.c_str()),
               0, (struct sockaddr *)&bc_address.bc_address, sizeof(bc_address.bc_address));
    }
}

int acceptClient(int server_socket)
{
    int client_fd;
    struct sockaddr_in client_address;
    int address_len = sizeof(client_address);
    client_fd = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t *)&address_len);
    return client_fd;
}

vector<Room> setupRooms(int port, const char *ip, int number_of_rooms)
{
    vector<Room> rooms;
    for (int i = 1; i <= number_of_rooms; i++)
    {
        struct Room new_room;
        new_room.room_socket = setupServer(port + i, ip);
        new_room.room_number = i;
        rooms.push_back(new_room);
    }
    return rooms;
}

void intiFileDescriptorSet(int &server_socket, fd_set &master_set, int &max_sd,
                           int rooms_number, vector<Room> &rooms, struct broadcast_info &bc_address)
{
    FD_ZERO(&master_set);
    max_sd = server_socket;
    FD_SET(server_socket, &master_set);

    for (int i = 0; i < rooms_number; i++)
    {
        FD_SET(rooms[i].room_socket, &master_set);
        if (rooms[i].room_socket > max_sd)
            max_sd = rooms[i].room_socket;
    }

    FD_SET(STDIN_FILENO, &master_set);
    if (STDIN_FILENO > max_sd)
        max_sd = STDIN_FILENO;

    FD_SET(bc_address.sock, &master_set);
    if (bc_address.sock > max_sd)
        max_sd = bc_address.sock;
}

void askPlayersToMove(vector<Room> &rooms, int current_room, int new_socket)
{
    if (!rooms[current_room].asked_player1_choice)
    {
        rooms[current_room].asked_player1_choice = true;
        rooms[current_room].player_1 = new_socket;
        send(rooms[current_room].player_1, FIRST_PLAYER_MESSAGE, strlen(FIRST_PLAYER_MESSAGE), 0);
    }
    else if (!rooms[current_room].asked_player2_choice)
    {
        rooms[current_room].asked_player2_choice = true;
        rooms[current_room].player_2 = new_socket;
        send(rooms[current_room].player_2, SECOND_PLAYER_MESSAGE, strlen(SECOND_PLAYER_MESSAGE), 0);
        send(rooms[current_room].player_1, CHOOSE_MOVE_MESSAGE, strlen(CHOOSE_MOVE_MESSAGE), 0);
        std::this_thread::sleep_for(chrono::milliseconds(10));
        send(rooms[current_room].player_2, CHOOSE_MOVE_MESSAGE, strlen(CHOOSE_MOVE_MESSAGE), 0);
    }
}

pair<string, string> extractPatternAndMessage(const string &str)
{
    pair<string, string> info;
    size_t first_star = str.find(STAR.c_str());
    size_t second_star = str.find(STAR.c_str(), first_star + 1);

    info.first = str.substr(first_star + 1, second_star - first_star - 1);
    info.second = str.substr(second_star + 1);

    return info;
}

int getRoomNumber(int rooms_number, int socket, const vector<struct Room> &rooms)
{
    for (int i = 0; i < rooms_number; i++)
        if (socket == rooms[i].room_socket)
        {
            return i;
        }

    return ROOM_NOT_FOUND;
}

bool isFull(Room room)
{
    return room.players.size() == MAX_CLIENTS;
}

string generateRoomList(int rooms_number, const std::vector<struct Room> &rooms)
{
    string msg_to_send;

    for (int i = 0; i < rooms_number; i++)
    {
        if (!isFull(rooms[i]))
            msg_to_send = msg_to_send + "\nRoom " + to_string(i + 1);
    }
    msg_to_send = msg_to_send + "\n";

    return msg_to_send;
}

void saveAndConnectToTheRoom(vector<Room> &rooms, int current_room, int rooms_number, int port, int &socket_num)
{
    if (isFull(rooms[current_room]))
    {
        string msg_to_send = ROOM_UNAVAILABLE_MESSAGE + generateRoomList(rooms_number, rooms);
        send(socket_num, msg_to_send.c_str(), strlen(msg_to_send.c_str()), 0);
    }
    else
    {
        rooms[current_room].players.push_back(socket_num);
        string msg_to_send = STAR + CONNECT_WITH_ROOM_OPC + STAR + to_string(port + current_room + 1);
        send(socket_num, msg_to_send.c_str(), strlen(msg_to_send.c_str()), 0);
    }
}

vector<string> extractInfo(const string &str)
{
    vector<string> result;
    size_t start = 0;
    size_t end = str.find(STAR);

    while (end != string::npos)
    {
        result.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(STAR, start);
    }
    result.push_back(str.substr(start));

    return result;
}

int judge(int p1, int p2)
{
    if (p1 == p2)
        return DRAW;
    if ((p2 == static_cast<int>(Move::NONE)) ||
        (((p2 == static_cast<int>(Move::ROCK))) && (p1 == static_cast<int>(Move::PAPER))) ||
        ((p2 == static_cast<int>(Move::PAPER)) && (p1 == static_cast<int>(Move::SCISSORS))) ||
        ((p2 == static_cast<int>(Move::SCISSORS)) && (p1 == static_cast<int>(Move::ROCK))))
        return PLAYER_1;
    else
        return PLAYER_2;
}

void signalHandler(int signal)
{
    exit(EXIT_SUCCESS);
}

void clearRoomSession(vector<Room> &rooms, int needed_room, fd_set &master_set)
{
    FD_CLR(rooms[needed_room].player_1, &master_set);
    close(rooms[needed_room].player_1);

    FD_CLR(rooms[needed_room].player_2, &master_set);
    close(rooms[needed_room].player_2);

    rooms[needed_room].asked_player1_choice = false;
    rooms[needed_room].asked_player2_choice = false;

    rooms[needed_room].p1_choice = rooms[needed_room].p2_choice = rooms[needed_room].winner = FAILED;
    rooms[needed_room].players.clear();
}

string examineGameResult(vector<Room> rooms, map<int, string> players_names, map<string, int> &players_win_count, int needed_room)
{
    string message;
    if (rooms[needed_room].winner != DRAW)
    {
        string name_of_winner = players_names[rooms[needed_room].players[rooms[needed_room].winner - 1]];
        players_win_count[name_of_winner]++;

        message = STAR + JUDGE_RESULT_OPC + STAR + "\nPlayer " + name_of_winner + 
                  " in the room " + to_string(needed_room + 1) + " won the game!\n";
    }
    else
    {
        message = STAR + JUDGE_RESULT_OPC + STAR + "\nThe game in the room " + to_string(needed_room + 1) +
                  " was a tie!\n";
    }
    return message;
}

int checkGameISReady(vector<Room> &rooms, int needed_room, int player_num, int choice)
{
    int all_playes_has_moved = 0;
    if (player_num == PLAYER_1)
    {
        rooms[needed_room].p1_choice = choice;
        if (rooms[needed_room].p2_choice != None)
            all_playes_has_moved = 1;
    }
    else if (player_num == PLAYER_2)
    {
        rooms[needed_room].p2_choice = choice;
        if (rooms[needed_room].p1_choice != None)
            all_playes_has_moved = 1;
    }
    return all_playes_has_moved;
}

int main(int argc, char const *argv[])
{
    if (argc != NUM_SERVER_INPUTS)
    {
        logError(SERVER_INPUT_ERROR.c_str());
        exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int rooms_number = atoi(argv[3]);

    int server_socket = setupServer(port, ip);
    struct broadcast_info bc_address = connectBroadcastSocket();
    logInfo(CONNECTION_ENABLED.c_str());

    vector<struct Room> rooms = setupRooms(port, ip, rooms_number);

    int max_sd;
    fd_set master_set, working_set;
    intiFileDescriptorSet(server_socket, master_set, max_sd, rooms_number, rooms, bc_address);

    map<string, int> players_win_count;
    map<int, string> players_names;
    char buffer[BUFFER_SIZE] = {0};
    int new_socket;

    signal(SIGINT, signalHandler);
    while (1)
    {
        working_set = master_set;
        select(max_sd + 1, &working_set, NULL, NULL, NULL);

        for (int i = 0; i <= max_sd; i++)
        {
            if (FD_ISSET(i, &working_set))
            {
                if (i == STDIN_FILENO)
                    terminateGame(buffer, players_win_count, bc_address);

                int current_room = getRoomNumber(rooms_number, i, rooms);
                if (i == server_socket)
                {
                    new_socket = acceptClient(server_socket);
                    FD_SET(new_socket, &master_set);
                    if (new_socket > max_sd)
                        max_sd = new_socket;
                    send(new_socket, RECEIVE_NAME_MESSAGE, strlen(RECEIVE_NAME_MESSAGE), 0);
                }
                else if (current_room != ROOM_NOT_FOUND)
                {
                    new_socket = acceptClient(rooms[current_room].room_socket);
                    FD_SET(new_socket, &master_set);
                    if (new_socket > max_sd)
                        max_sd = new_socket;

                    askPlayersToMove(rooms, current_room, new_socket);
                }
                else
                {
                    recv(i, buffer, BUFFER_SIZE, 0);
                    string new_message(buffer), pattern, message;

                    pair<string, string> info = extractPatternAndMessage(new_message);
                    pattern = info.first;
                    message = info.second;

                    if (pattern == NAME_INP_OPC)
                    {
                        message.pop_back();
                        players_names[i] = message;
                        players_win_count[message] = 0;
                        string msg_to_send = RECEIVE_ROOM_NUMBER_MESSAGE + generateRoomList(rooms_number, rooms);
                        send(i, msg_to_send.c_str(), strlen(msg_to_send.c_str()), 0);
                    }

                    else if (pattern == ROOM_CHOOSE_OPC)
                    {
                        int current_room = stoi(message) - 1;
                        saveAndConnectToTheRoom(rooms, current_room, rooms_number, port, i);
                    }

                    else if (pattern == ENCODED_CHOICE_OPC)
                    {
                        vector<string> info = extractInfo(message);
                        int room_num = stoi(info[0]);
                        int player_num = stoi(info[1]);
                        int choice = stoi(info[2]);

                        int all_playes_has_moved = 0, needed_room = room_num - 1;

                        all_playes_has_moved = checkGameISReady(rooms, needed_room, player_num, choice);

                        if (all_playes_has_moved)
                        {
                            rooms[needed_room].winner = judge(rooms[needed_room].p1_choice, rooms[needed_room].p2_choice);

                            string msg_to_send = examineGameResult(rooms, players_names, players_win_count, needed_room);

                            sendto(bc_address.sock, msg_to_send.c_str(), strlen(msg_to_send.c_str()),
                                   0, (struct sockaddr *)&bc_address.bc_address, sizeof(bc_address.bc_address));

                            this_thread::sleep_for(chrono::milliseconds(TIME_OUT));

                            clearRoomSession(rooms, needed_room, master_set);

                            vector<int> former_room_players;

                            for (int k = 0; k < 2; k++)
                                former_room_players.push_back(rooms[needed_room].players[k]);

                            for (int k = 0; k < 2; k++)
                            {
                                string new_msg_to_send = RECEIVE_ROOM_NUMBER_MESSAGE + generateRoomList(rooms_number, rooms);
                                send(former_room_players[k], new_msg_to_send.c_str(), strlen(new_msg_to_send.c_str()), 0);
                            }
                        }
                    }

                    else if (pattern == JUDGE_RESULT_OPC || pattern == FINAL_MSG_OPC)
                    {
                        logMsg(message.c_str());
                        if (pattern == FINAL_MSG_OPC)
                            exit(EXIT_SUCCESS);
                    }

                    memset(buffer, 0, BUFFER_SIZE);
                }
            }
        }
    }

    return 0;
}