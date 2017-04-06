#define _GNU_SOURCE

#include <fcntl.h>
#include <getopt.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

#define MAX_FORK_MAGIC 10

typedef struct {
    int writePipe;
    int readPipe;
}__attribute__((packed)) pipeDes;

typedef struct {
    local_id localID;
    int nChild;
    pipeDes pDes[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
} __attribute__((packed)) LocalInfo;

int logToFile(FILE *fp, const char *format, ...) {
    va_list args;
    va_start (args, format);
    vprintf(format, args);
    va_start (args, format);
    vfprintf(fp, format, args);
    fflush(fp);
    va_end (args);
    return EXIT_SUCCESS;
}

int send(void *self, local_id dst, const Message *msg) {
    LocalInfo *info = self;
    size_t n = sizeof(msg->s_header) + msg->s_header.s_payload_len;
    ssize_t res = write(info->pDes[info->localID][dst].writePipe, msg, n);
    printf("From localID=%d to %d write %d\n", info->localID, dst, (int) res);
    return EXIT_SUCCESS;
}

int send_multicast(void *self, const Message *msg) {
    LocalInfo *info = self;
    for (local_id i = 0; i <= info->nChild; ++i) {
        if (i != info->localID) {
            size_t n = sizeof(msg->s_header) + msg->s_header.s_payload_len;
            ssize_t res = write(info->pDes[info->localID][i].writePipe, msg, n);
            if (res != n) return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    LocalInfo *info = malloc(sizeof(LocalInfo));
    info->localID = 0;
    info->nChild = 8;
    memset(info->pDes, -1, sizeof(info->pDes));
    int opt, i;
    pid_t pid = 1;
    FILE *eventFile = fopen(events_log, "a");//TODO Change fopen to open
    if (eventFile == NULL) {
        fprintf(stderr, "Cannot open events file.\n");
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
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        logToFile(eventFile, "\nTime: %s", asctime(timeinfo));
        logToFile(eventFile, "nChild=%d\n", info->nChild);
        logToFile(eventFile, "argc=%d\n", argc);
        for (i = 0; i < argc; ++i) {
            logToFile(eventFile, "argv[%d]=%s\n", i, argv[i]);
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
        }

    }

    Message myMsg;
    memset(&myMsg, 0, sizeof(Message));
    snprintf(myMsg.s_payload, MAX_PAYLOAD_LEN, log_started_fmt, info->localID, getpid(), getppid());
    myMsg.s_header.s_payload_len = (uint16_t) strlen(myMsg.s_payload);
    myMsg.s_header.s_magic = MESSAGE_MAGIC;
    myMsg.s_header.s_local_time = 0L;
    myMsg.s_header.s_type = DONE;

    logToFile(eventFile, log_started_fmt, info->localID, getpid(), getppid());

    send(info, (local_id) (info->localID + 1) % (info->nChild + 1), &myMsg);
    //
    fclose(eventFile);
    return EXIT_SUCCESS;
}
