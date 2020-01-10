//
// Created by razorr on 15.04.17.
//

#ifndef INC_DISTRIBUTED_LAB_BASA62_FUNCTIONS_H
#define INC_DISTRIBUTED_LAB_BASA62_FUNCTIONS_H 1

#include <unistd.h>
#include "ipc.h"

static const size_t sizeHeader = sizeof(MessageHeader);
static const size_t sizeMessage = sizeof(Message);

enum {
    MAX_LOOP = 5
};

typedef struct {
    int readPipe;
    int writePipe;
}__attribute__((packed)) PipeDes;

typedef struct {
    local_id localID, lastMsgPid;
    int nChild, eventFd, logFd, lab, mutexEnabled, doneChildren;
    pid_t pid;
    pid_t pPid;
    PipeDes pDes[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
    timestamp_t timeVector[MAX_PROCESS_ID + 1];
} __attribute__((packed)) LocalInfo;

int logToFile(int fd, const char *format, ...);

int exitWithError(int fd, int status);

int receiveAll(LocalInfo *info);

int openPipes(LocalInfo *info);

int closeUnnecessaryPipes(LocalInfo *info);

int closeUsedPipes(LocalInfo *info);

int preFork(LocalInfo *info);

void setMessage(Message *msg, MessageType type, uint16_t length);

LocalInfo *pInfo;

void increment_vector_time();

void set_vector_time(const Message *msg);

timestamp_t get_vector_time();

#endif //INC_DISTRIBUTED_LAB_BASA62_FUNCTIONS_H
