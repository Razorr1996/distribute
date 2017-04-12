#define _GNU_SOURCE

#include <fcntl.h>
#include <getopt.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <wait.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

#define MAX_FORK_MAGIC 10

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

int main(int argc, char *argv[]) {
    LocalInfo *info = malloc(sizeof(LocalInfo));
    info->localID = 0;
    info->nChild = 8;
    info->pid = getpid();
    info->pPid = getppid();
    info->eventFd = open(events_log, O_CREAT | O_WRONLY | O_APPEND);
    if (info->eventFd == -1) {
        exitWithError(info->eventFd, errno);
    }
    memset(info->pDes, -1, sizeof(info->pDes));
    int opt = getopt(argc, argv, "p:");
    if (opt != -1 && optarg != NULL) {
        int nChildTmp;
        sscanf(optarg, "%d", &nChildTmp);
        nChildTmp = nChildTmp > MAX_FORK_MAGIC ? MAX_FORK_MAGIC : nChildTmp;
        nChildTmp = nChildTmp > MAX_PROCESS_ID ? MAX_PROCESS_ID : nChildTmp;
        info->nChild = nChildTmp > 0 ? nChildTmp : info->nChild;
    }
    {//debug output
        time_t rawTime;
        struct tm *timeInfo;
        time(&rawTime);
        timeInfo = localtime(&rawTime);
        logToFile(info->eventFd, "\nTime: %s", asctime(timeInfo));
        logToFile(info->eventFd, "Children=%d\n", info->nChild);
        fsync(1);
        fsync(info->eventFd);
    }

    if (openPipes(info) != EXIT_SUCCESS) exitWithError(info->eventFd, errno);

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

    closeUnnecessaryPipes(info);

    if (info->localID != PARENT_ID) {
        Message myMsg;
        memset(&myMsg, 0, sizeMessage);
        snprintf(myMsg.s_payload, MAX_PAYLOAD_LEN, log_started_fmt, info->localID, info->pid, info->pPid);
        myMsg.s_header.s_magic = MESSAGE_MAGIC;
        myMsg.s_header.s_payload_len = (uint16_t) strlen(myMsg.s_payload);
        myMsg.s_header.s_type = STARTED;
        myMsg.s_header.s_local_time = 0L;

        logToFile(info->eventFd, myMsg.s_payload);

        send_multicast(info, &myMsg);
    }

    receiveAll(info);
    logToFile(info->eventFd, log_received_all_started_fmt, info->localID);
    if (info->localID != PARENT_ID) {
        Message myMsg;
        memset(&myMsg, 0, sizeMessage);
        snprintf(myMsg.s_payload, MAX_PAYLOAD_LEN, log_done_fmt, info->localID);
        myMsg.s_header.s_magic = MESSAGE_MAGIC;
        myMsg.s_header.s_payload_len = (uint16_t) strlen(myMsg.s_payload);
        myMsg.s_header.s_type = DONE;
        myMsg.s_header.s_local_time = 0L;

        logToFile(info->eventFd, myMsg.s_payload);

        send_multicast(info, &myMsg);
    }
    receiveAll(info);
    logToFile(info->eventFd, log_received_all_done_fmt, info->localID);
    closeUsedPipes(info);
    if (info->localID == PARENT_ID) {
        for (int j = 0; j < info->nChild; ++j) {
            wait(NULL);
        }
        logToFile(info->eventFd, "All children end\n");
    }
    //
    close(info->eventFd);
    return EXIT_SUCCESS;
}
