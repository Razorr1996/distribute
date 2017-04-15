#define _GNU_SOURCE

#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <wait.h>
#include <unistd.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

#include "functions.h"

int main(int argc, char *argv[]) {
    LocalInfo *info = malloc(sizeof(LocalInfo));
    info->localID = 0;
    info->nChild = 8;
    info->pid = getpid();
    info->pPid = getppid();
    info->eventFd = open(events_log, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (info->eventFd == -1) {
        exitWithError(info->eventFd, errno);
    }
    memset(info->pDes, -1, sizeof(info->pDes));
    int opt = getopt(argc, argv, "p:");
    if (opt != -1 && optarg != NULL) {
        int nChildTmp;
        sscanf(optarg, "%d", &nChildTmp);
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

    preFork(info);

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
