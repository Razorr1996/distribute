//
// Created by razorr on 25.05.17.
//

#define _GNU_SOURCE

#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <wait.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "banking.h"

#include "functions.h"
#include "phil.h"

Phil *phil = NULL;

int child_lap(LocalInfo *info) {
    int res;
    Message msg;
    res = receive_any(info, &msg);
    if (res != EXIT_SUCCESS) return res;

    set_lamport_time(msg.s_header.s_local_time);
    increment_lamport_time();
    phil_print(info, phil);
    switch (msg.s_header.s_type) {
        case CS_REQUEST:
            phil->reqf[info->lastMsgPid] = 1;
            phil_print(info, phil);
            assert(phil->fork[info->lastMsgPid]);
            if (phil->fork[info->lastMsgPid] && phil->dirty[info->lastMsgPid]) {
                phil->fork[info->lastMsgPid] = phil->dirty[info->lastMsgPid] = 0;
                increment_lamport_time();
                setMessage(&msg, CS_REPLY, 0);
                res = send(info, info->lastMsgPid, &msg);
                if (res != EXIT_SUCCESS) return res;
            }
            break;
        case CS_REPLY:
            phil->fork[info->lastMsgPid] = 1;
            phil->dirty[info->lastMsgPid] = 0;
            break;
        case DONE:
            info->doneChildren++;
            break;
    }
    phil_print(info, phil);
    return EXIT_SUCCESS;
}

int request_cs(const void *self) {
    LocalInfo *info = (LocalInfo *) self;
    int res = 0;
    Message msg;

    if (phil == NULL) phil_init(info, &phil);
    phil_print(info, phil);

    while (phil_check_forks(info, phil) != EXIT_SUCCESS) {
        phil_print(info, phil);
        for (int i = 1; i <= info->nChild; ++i) {
            if (i == info->localID) continue;
            if (!phil->fork[i] && phil->reqf[i]) {
                phil->reqf[i] = 0;
                increment_lamport_time();
                setMessage(&msg, CS_REQUEST, 0);
                res = send(info, i, &msg);
                if (res != EXIT_SUCCESS) return res;
            }
        }
        res = child_lap(info);
        if (res != EXIT_SUCCESS) return res;
    }
    return EXIT_SUCCESS;
}

int release_cs(const void *self) {
    LocalInfo *info = (LocalInfo *) self;
    int res;
    Message msg;
    phil_print(info, phil);
    phil_set_all_dirty(info, phil);
    phil_print(info, phil);

    for (int i = 1; i <= info->nChild; ++i) {
        if (i == info->localID) continue;
        if (phil->reqf[i] && phil->fork[i] && phil->dirty[i]) {
            phil->fork[i] = phil->dirty[i] = 0;
            increment_lamport_time();
            setMessage(&msg, CS_REPLY, 0);
            res = send(info, i, &msg);
            if (res != EXIT_SUCCESS) return res;
        }
    }
    phil_print(info, phil);
    return EXIT_SUCCESS;
}


void childStartedMsg(LocalInfo *info, Message *myMsg) {
    memset(myMsg, 0, sizeMessage);
    myMsg->s_header.s_magic = MESSAGE_MAGIC;
    myMsg->s_header.s_type = STARTED;
    myMsg->s_header.s_local_time = get_lamport_time();
    snprintf(myMsg->s_payload, MAX_PAYLOAD_LEN, log_started_fmt, get_lamport_time(), info->localID, info->pid,
             info->pPid, 0);
    myMsg->s_header.s_payload_len = (uint16_t) strlen(myMsg->s_payload);
}

int child(LocalInfo *info) {
    Message msg;
    closeUnnecessaryPipes(info);
    {
        childStartedMsg(info, &msg);
        logToFile(info->eventFd, msg.s_payload);
        logToFile(info->eventFd, "%d\n", msg.s_header.s_type);
        send_multicast(info, &msg);
    }
    receiveAll(info);

    logToFile(info->eventFd, log_received_all_started_fmt, get_lamport_time(), info->localID);

    {//Work
        if (info->mutexEnabled) {
            request_cs(info);
        }
        local_id loop_count = info->localID * MAX_LOOP;
        for (int i = 0; i < loop_count; ++i) {
            snprintf(msg.s_payload, MAX_PAYLOAD_LEN, log_loop_operation_fmt, info->localID, i + 1, loop_count);
//            logToFile(info->eventFd, msg.s_payload);
            print(msg.s_payload);
        }
        if (info->mutexEnabled)
            release_cs(info);
    }

    {//Finish
        memset(&msg, 0, sizeMessage);
        snprintf(msg.s_payload, MAX_PAYLOAD_LEN, log_done_fmt, get_lamport_time(),
                 info->localID,
                 0);
        msg.s_header.s_magic = MESSAGE_MAGIC;
        msg.s_header.s_payload_len = (uint16_t) strlen(msg.s_payload);
        msg.s_header.s_type = DONE;
        msg.s_header.s_local_time = get_lamport_time();

        logToFile(info->eventFd, msg.s_payload);

        send_multicast(info, &msg);
    }
//    receiveAll(info);
    int res = 0;
    logToFile(info->eventFd, "Child %d, done %d\n", info->localID, info->doneChildren);
    while (info->doneChildren < info->nChild - 1) {
        phil_print(info, phil);
        res = child_lap(info);
        if (res != EXIT_SUCCESS) return res;
    }
    logToFile(info->eventFd, log_received_all_done_fmt, get_lamport_time(), info->localID);


    closeUsedPipes(info);
    close(info->eventFd);
    return EXIT_SUCCESS;
}

int parent(LocalInfo *info) {
    Message msg;
    closeUnnecessaryPipes(info);

//    receiveAll(info);
//
//    logToFile(info->eventFd, log_received_all_started_fmt, get_lamport_time(), info->localID);
    //Parent work
    int res;
    while (info->doneChildren < info->nChild) {
        res = receive_any(info, &msg);
        if (res != EXIT_SUCCESS) return res;
//        logToFile(info->eventFd, msg.s_payload);
        if (msg.s_header.s_type == DONE) info->doneChildren++;
    }
    closeUsedPipes(info);
    //End Parent work
    for (int j = 0; j < info->nChild; ++j) {
        wait(NULL);
    }
    logToFile(info->eventFd, "All children end\n");
    close(info->eventFd);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    LocalInfo *info = malloc(sizeof(LocalInfo));
    info->lab = 4;
    info->localID = 0;
    info->nChild = 8;
    info->pid = getpid();
    info->pPid = getppid();
    info->mutexEnabled = 0;
    info->doneChildren = 0;
    info->eventFd = open(events_log, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (info->eventFd == -1) {
        exitWithError(info->eventFd, errno);
    }
    memset(info->pDes, -1, sizeof(info->pDes));
    {//parse arguments
        int opt, nChildTmp;
        struct option long_opts[] = {{"mutexl", no_argument, NULL, 'm'},
                                     {NULL, 0,               NULL, 0}};
        while ((opt = getopt_long(argc, argv, "p:", long_opts, NULL)) != -1) {
            switch (opt) {
                case 0:
                    break;
                case 'm':
                    info->mutexEnabled = 1;
                    break;
                case 'p':
                    sscanf(optarg, "%d", &nChildTmp);
                    nChildTmp = nChildTmp > MAX_PROCESS_ID ? MAX_PROCESS_ID : nChildTmp;
                    info->nChild = nChildTmp > 0 ? nChildTmp : info->nChild;
                    break;
                case -1:
                    return -1;
            }
        }
        logToFile(info->eventFd, "\nMutexl: %d, processes: %d", info->mutexEnabled, info->nChild);
//        exit(0);
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

    if (info->localID != PARENT_ID) {
        child(info);
    } else {
        parent(info);
    }
    return EXIT_SUCCESS;
}
