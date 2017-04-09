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

#include "common.h"
#include "ipc.h"
#include "pa1.h"

#define MAX_FORK_MAGIC 10

static size_t sizeHeader = sizeof(MessageHeader);
static size_t sizeMessage = sizeof(Message);

typedef struct {
    int writePipe;
    int readPipe;
}__attribute__((packed)) pipeDes;

typedef struct {
    local_id localID;
    int nChild;
    pid_t pid;
    pid_t pPid;
    pipeDes pDes[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
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

int send(void *self, local_id dst, const Message *msg) {
    LocalInfo *info = self;
    size_t n = sizeof(msg->s_header) + msg->s_header.s_payload_len;
    ssize_t res = write(info->pDes[info->localID][dst].writePipe, msg, n);
    if (res != n) return EXIT_FAILURE;
//    printf("From localID=%d to %d write %d\n", info->localID, dst, (int) res);
    return EXIT_SUCCESS;
}

int send_multicast(void *self, const Message *msg) {
    LocalInfo *info = self;
    for (local_id i = 0; i <= info->nChild; ++i) {
        if (i != info->localID) {
            size_t n = sizeof(msg->s_header) + msg->s_header.s_payload_len;
            ssize_t res = write(info->pDes[info->localID][i].writePipe, msg, n);
            if (res != n) return EXIT_FAILURE;
//            printf("From localID=%d to %d write %d\n", info->localID, i, (int) res);
        }
    }
    return EXIT_SUCCESS;
}

int receive(void *self, local_id from, Message *msg) {//TODO Do it
    LocalInfo *info = self;
    ssize_t res, res1;
    do {
        res = read(info->pDes[from][info->localID].readPipe, msg, sizeHeader);
    } while (res == -1 && errno == EAGAIN);
    do {
        res1 = read(info->pDes[from][info->localID].readPipe, msg + sizeHeader, (size_t) res);
    } while (res1 == -1 && errno == EAGAIN);
    return EXIT_SUCCESS;
}

int receive_any(void *self, Message *msg) {//TODO Do it
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    LocalInfo *info = malloc(sizeof(LocalInfo));
    info->localID = 0;
    info->nChild = 8;
    info->pid = getpid();
    info->pPid = getppid();
    memset(info->pDes, -1, sizeof(info->pDes));
    int opt, i;
    pid_t pid = 1;
    int eventFd = open(events_log, O_CREAT | O_WRONLY | O_APPEND);
    if (eventFd == -1) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }
    opt = getopt(argc, argv, "p:");
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
        logToFile(eventFd, "\nTime: %s", asctime(timeInfo));
        logToFile(eventFd, "nChild=%d\n", info->nChild);
        logToFile(eventFd, "argc=%d\n", argc);
        for (i = 0; i < argc; ++i) {
            logToFile(eventFd, "argv[%d]=%s\n", i, argv[i]);
        }
    }

    for (i = 0; i <= info->nChild; ++i) {
        for (int j = 0; j <= info->nChild; ++j) {
            int fDes[2];
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

    for (i = 1; i <= info->nChild && pid != 0; ++i) {
        pid = fork();
        if (pid == 0) {
            info->localID = (local_id) i;
            info->pPid = info->pid;
            info->pid = getpid();
        }

    }

    Message myMsg;
    memset(&myMsg, 0, sizeMessage);
    snprintf(myMsg.s_payload, MAX_PAYLOAD_LEN, log_started_fmt, info->localID, info->pid, info->pPid);
    myMsg.s_header.s_magic = MESSAGE_MAGIC;
    myMsg.s_header.s_payload_len = (uint16_t) strlen(myMsg.s_payload);
    myMsg.s_header.s_type = STARTED;
    myMsg.s_header.s_local_time = 0L;

    logToFile(eventFd, myMsg.s_payload);

    send_multicast(info, &myMsg);
    //
    close(eventFd);
    return EXIT_SUCCESS;
}
