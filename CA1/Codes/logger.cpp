#include "logger.h"

#include <iostream>
#include <unistd.h>
#include <cstring>
#include "ans_colours.h"

using namespace std;

void logError(const char *msg)
{
    write(STDERR_FILENO, ANS_RED "[Error] " ANS_RST, 8 + ANS_LEN);
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, "\n", 1);
}

void logMsg(const char *msg)
{
    write(STDOUT_FILENO, ANS_CYN "[Message] " ANS_RST, 10 + ANS_LEN);
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, "\n", 1);
}

void logInfo(const char *msg)
{
    write(STDOUT_FILENO, ANS_GRN "[Info] " ANS_RST, 7 + ANS_LEN);
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDOUT_FILENO, "\n", 1);
}