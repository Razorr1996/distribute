//
// Created by razorr on 15.04.17.
//

#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <unistd.h>

#include "functions.h"

int logToFile(int fd, const char *format, ...) {
    va_list args;
    va_start (args, format);
    vprintf(format, args);
    va_start (args, format);
    vdprintf(fd, format, args);
    va_end (args);
    return EXIT_SUCCESS;
}

int exitWithError(int fd, int status) {
    dprintf(fd, "ERROR: %s\n", strerror(status));
    fprintf(stderr, "%s\n", strerror(status));
    fflush(stderr);
    exit(status);
}

int send(void *self, local_id dst, const Message *msg) {
    LocalInfo *info = self;
    size_t n = sizeof(msg->s_header) + msg->s_header.s_payload_len;
    ssize_t res = write(info->pDes[info->localID][dst].writePipe, msg, n);
    if (res != n) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int send_multicast(void *self, const Message *msg) {
    LocalInfo *info = self;
    for (local_id i = 0; i <= info->nChild; ++i) {
        if (i != info->localID) {
            size_t n = sizeHeader + msg->s_header.s_payload_len;
            ssize_t res = write(info->pDes[info->localID][i].writePipe, msg, n);
            if (res != n && errno != EAGAIN) {
                exitWithError(info->eventFd, errno);
            }
        }
    }
    return EXIT_SUCCESS;
}

int receive(void *self, local_id from, Message *msg) {
    LocalInfo *info = self;
    ssize_t res = 0;
    size_t c1 = sizeHeader;
    do {
        if (res > 0) {
            c1 -= res;
        } else usleep(10000);
        res = read(info->pDes[from][info->localID].readPipe, msg, c1);
        if (res == -1 && errno != EAGAIN) {
            exitWithError(info->eventFd, errno);
        }
    } while (c1 > 0);
    c1 = msg->s_header.s_payload_len;
    do {
        if (res > 0) {
            c1 -= res;
        } else usleep(10000);
        res = read(info->pDes[from][info->localID].readPipe, &msg->s_payload, c1);
        if (res == -1) exitWithError(info->eventFd, errno);
    } while (c1 > 0);
    return EXIT_SUCCESS;
}

/*int receive_any(void *self, Message *msg) {//TODO To receive
    LocalInfo *info = self;
    ssize_t res, res1;
    int count = 1;
    int arrTmp[info->nChild + 1];
    memset(arrTmp, 0, sizeof(arrTmp));
    arrTmp[0] = 1;
    for (int from = 1; count > 0; from = (from + 1) % info->nChild + 1) {
        if (arrTmp[from]) {
            count--;
        }
    }
    return EXIT_SUCCESS;
}*/

int receiveAll(LocalInfo *info) {
    Message inMsg;
    memset(&inMsg, 0, sizeMessage);
    for (int i = 1; i <= info->nChild; i++) {
        if (i != info->localID) {
            receive(info, i, &inMsg);
            logToFile(info->eventFd, "Msg(%2d->%2d):\t%s", i, info->localID, inMsg.s_payload);
        }
    }
    return EXIT_SUCCESS;
}

int openPipes(LocalInfo *info) {
    int fDes[2];
    for (int i = 0; i <= info->nChild; ++i) {
        for (int j = 0; j <= info->nChild; ++j) {
            if (i != j) {
                if (pipe(fDes)) {
                    fprintf(stderr, "Cannot open pipe.\n");
                    exit(EXIT_FAILURE);
                } else {
                    fcntl(fDes[0], F_SETFL, fcntl(fDes[1], F_GETFL) | O_NONBLOCK);
                    info->pDes[i][j].readPipe = fDes[0];
                    info->pDes[i][j].writePipe = fDes[1];
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

int closeUnnecessaryPipes(LocalInfo *info) {
    logToFile(info->eventFd, "Process %2d start closing unnecessary pipes\n", info->localID);
    for (int i = 0; i <= info->nChild; ++i) {
        for (int j = 0; j <= info->nChild; ++j) {
            if (i == j) continue;
            if (i == info->localID) {
                close(info->pDes[i][j].readPipe);
                info->pDes[i][j].readPipe = -1;
                continue;
            }
            if (j == info->localID) {
                close(info->pDes[i][j].writePipe);
                info->pDes[i][j].writePipe = -1;
                continue;
            } else {
                close(info->pDes[i][j].readPipe);
                close(info->pDes[i][j].writePipe);
                info->pDes[i][j].readPipe = -1;
                info->pDes[i][j].writePipe = -1;
                continue;
            }
        }
    }
    logToFile(info->eventFd, "Process %2d end closing unnecessary pipes\n", info->localID);
    return EXIT_SUCCESS;
}

int closeUsedPipes(LocalInfo *info) {
    logToFile(info->eventFd, "Process %2d start closing used pipes\n", info->localID);
    for (int i = 0; i <= info->nChild; ++i) {
        if (i != info->localID) {
            close(info->pDes[info->localID][i].writePipe);
            close(info->pDes[i][info->localID].readPipe);
            info->pDes[info->localID][i].writePipe = -1;
            info->pDes[i][info->localID].readPipe = -1;
        }
    }
    logToFile(info->eventFd, "Process %2d end closing used pipes\n", info->localID);
    return EXIT_SUCCESS;
}

int preFork(LocalInfo *info) {
    logToFile(info->eventFd, "Fork[0]: %5d\n", info->pid);
    for (int i = 1, pid = 1; i <= info->nChild && pid != 0; ++i) {
        pid = fork();
        if (pid == 0) {
            info->localID = (local_id) i;
            info->pPid = info->pid;
            info->pid = getpid();
        } else {
            logToFile(info->eventFd, "Fork[%d]: %5d\n", i, pid);
        }
    }
    return EXIT_SUCCESS;
}
