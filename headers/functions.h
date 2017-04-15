//
// Created by razorr on 15.04.17.
//

#ifndef INC_DISTRIBUTED_LAB_BASA62_FUNCTIONS_H
#define INC_DISTRIBUTED_LAB_BASA62_FUNCTIONS_H 1

#include <unistd.h>
#include "ipc.h"

static const size_t sizeHeader = sizeof(MessageHeader);
static const size_t sizeMessage = sizeof(Message);

typedef struct {
    int readPipe;
    int writePipe;
}__attribute__((packed)) PipeDes;

typedef struct {
    local_id localID;
    int nChild, eventFd, logFd;
    pid_t pid;
    pid_t pPid;
    PipeDes pDes[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
} __attribute__((packed)) LocalInfo;

int logToFile(int fd, const char *format, ...);

int exitWithError(int fd, int status);

int receiveAll(LocalInfo *info);

int openPipes(LocalInfo *info);

int closeUnnecessaryPipes(LocalInfo *info);

int closeUsedPipes(LocalInfo *info);

int preFork(LocalInfo *info);

#endif //INC_DISTRIBUTED_LAB_BASA62_FUNCTIONS_H