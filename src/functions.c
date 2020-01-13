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
#include <assert.h>

#include "functions.h"
#include "banking.h"

void increment_vector_time() {
    pInfo->timeVector[pInfo->localID]++;
}

void set_vector_time(const Message *msg) {
    for (int i = 0; i <= pInfo->nChild; ++i) {
        if (pInfo->timeVector[i] < msg->s_header.s_local_timevector[i])
            pInfo->timeVector[i] = msg->s_header.s_local_timevector[i];
    }
}

timestamp_t get_vector_time() {
    return pInfo->timeVector[pInfo->localID];
}

timestamp_t localTime = 0;

timestamp_t get_lamport_time() {
    return localTime;
}

void set_lamport_time(timestamp_t newTime) {
    localTime = newTime > localTime ? newTime : localTime;
}

void increment_lamport_time() {
    localTime++;
}

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
    if (dst == info->localID)
        return EXIT_SUCCESS;
#ifdef _VT
    memcpy(msg->s_header.s_local_timevector, pInfo->timeVector, (MAX_PROCESS_ID + 1) * sizeof(timestamp_t));
#endif
    size_t n = sizeHeader + msg->s_header.s_payload_len;
    ssize_t res = write(info->pDes[info->localID][dst].writePipe, msg, n);
    if (res != n && errno != EAGAIN) {
        exitWithError(info->eventFd, errno);
    }
    return EXIT_SUCCESS;
}

int send_multicast(void *self, const Message *msg) {
    LocalInfo *info = self;
#ifdef _VT
    memcpy(msg->s_header.s_local_timevector, pInfo->timeVector, (MAX_PROCESS_ID + 1) * sizeof(timestamp_t));
#endif
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
    if (from == info->localID)
        return EXIT_SUCCESS;
    while (1) {
        ssize_t res = read(info->pDes[from][info->localID].readPipe, msg, sizeHeader);
        if (res > 0) {
            assert(msg->s_header.s_magic == MESSAGE_MAGIC);
            while (1) {
                res = read(info->pDes[from][info->localID].readPipe, msg->s_payload, msg->s_header.s_payload_len);
                if (res >= 0) {
#ifdef _VT
                    set_vector_time(msg);
                    switch (msg->s_header.s_type) {
                        case SNAPSHOT_VTIME:
                        case SNAPSHOT_ACK:
                            break;
                        default:
                            increment_vector_time();
                            break;
                    }
#endif
                    return EXIT_SUCCESS;
                }
            }
        } else usleep(10000);
    }
    return EXIT_SUCCESS;
}

int receive_any(void *self, Message *msg) {
    LocalInfo *info = self;
    while (1) {
        for (int from = 0; from <= info->nChild; from++) {
            if (from == info->localID) continue;
            ssize_t res = read(info->pDes[from][info->localID].readPipe, msg, sizeHeader);
            if (res > 0) {
                info->lastMsgPid = from;
                assert(msg->s_header.s_magic == MESSAGE_MAGIC);
                while (1) {
                    res = read(info->pDes[from][info->localID].readPipe, msg->s_payload, msg->s_header.s_payload_len);
                    if (res >= 0) {
#ifdef _VT
                        set_vector_time(msg);
                        switch (msg->s_header.s_type) {
                            case SNAPSHOT_VTIME:
                            case SNAPSHOT_ACK:
                                break;
                            default:
                                increment_vector_time();
                                break;
                        }
#endif
                        return EXIT_SUCCESS;
                    }
                }
            }
        }
        usleep(10000);
    }
    return EXIT_SUCCESS;
}

int receiveAll(LocalInfo *info) {
    Message inMsg;
    for (int i = 1; i <= info->nChild; i++) {
        if (i != info->localID) {
            receive(info, i, &inMsg);
            logToFile(info->eventFd, "Msg(%2d->%2d):\t%s", i, info->localID, inMsg.s_payload);
        }
#ifndef _VT
        if (info->lab >= 3) {
            set_lamport_time(inMsg.s_header.s_local_time);
            increment_lamport_time();
        }
#endif
    }
    return EXIT_SUCCESS;
}

int openPipes(LocalInfo *info) {
    int fDes[2];
    for (int i = 0; i <= info->nChild; ++i) {
        for (int j = 0; j <= info->nChild; ++j) {
            if (i != j) {
                if (pipe2(fDes, O_NONBLOCK)) {
                    fprintf(stderr, "Cannot open pipe.\n");
                    exit(EXIT_FAILURE);
                } else {
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
            if (i != info->localID) {
                close(info->pDes[i][j].writePipe);
                info->pDes[i][j].writePipe = -1;
            }
            if (j != info->localID) {
                close(info->pDes[i][j].readPipe);
                info->pDes[i][j].readPipe = -1;
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

void setMessage(Message *msg, MessageType type, uint16_t length) {
    memset(msg, 0, sizeMessage);
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_type = type;
#ifdef _VT
    msg->s_header.s_local_time = get_vector_time();
#else
    msg->s_header.s_local_time = get_lamport_time();
#endif
    msg->s_header.s_payload_len = length;
}
