//
// Created by razorr on 15.04.17.
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
#include <pa1_starter_code/ipc.h>
#include <pa2345_starter_code/banking.h>
#include <functions.h>

#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "banking.h"

#include "functions.h"

extern void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    Message msg;
    memset(&msg, 0, sizeMessage);

    msg.s_header.s_type = TRANSFER;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_local_time = get_physical_time();

    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;
    msg.s_header.s_payload_len = sizeof(order);
    memcpy(msg.s_payload, &order, msg.s_header.s_payload_len);

    int res = send(parent_data, src, &msg);
    if (res != EXIT_SUCCESS) exitWithError(((LocalInfo *) parent_data)->logFd, errno);

    res = receive(parent_data, dst, &msg);
    if (res != EXIT_SUCCESS) exitWithError(((LocalInfo *) parent_data)->logFd, errno);
}

int updateBalance(BalanceHistory history, TransferOrder order) {
    timestamp_t nowTime = get_physical_time();
    if (nowTime > 0)
        for (timestamp_t time = history.s_history_len; time <= nowTime; time++) {
            history.s_history[time] = history.s_history[time - 1];
            history.s_history[time].s_time++;
        }
    history.s_history[nowTime].s_balance += (order.s_dst == history.s_id ? 1 : -1) * order.s_amount;
    history.s_history_len = (uint8_t) (nowTime + 1);
    return EXIT_SUCCESS;
}

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

    //TODO Clear it
    if (info->localID != PARENT_ID) {
        Message myMsg;
        memset(&myMsg, 0, sizeMessage);
        snprintf(myMsg.s_payload, MAX_PAYLOAD_LEN, log_started_fmt, 0, info->localID, info->pid, info->pPid, 0);
        myMsg.s_header.s_magic = MESSAGE_MAGIC;
        myMsg.s_header.s_payload_len = (uint16_t) strlen(myMsg.s_payload);
        myMsg.s_header.s_type = STARTED;
        myMsg.s_header.s_local_time = 0L;

        logToFile(info->eventFd, myMsg.s_payload);

        send_multicast(info, &myMsg);
    }

    receiveAll(info);

    //TODO Clear it
    logToFile(info->eventFd, log_received_all_started_fmt, get_physical_time(), info->localID);
    if (info->localID != PARENT_ID) {
        Message myMsg;
        memset(&myMsg, 0, sizeMessage);
        snprintf(myMsg.s_payload, MAX_PAYLOAD_LEN, log_done_fmt, get_physical_time(), info->localID, 0);
        myMsg.s_header.s_magic = MESSAGE_MAGIC;
        myMsg.s_header.s_payload_len = (uint16_t) strlen(myMsg.s_payload);
        myMsg.s_header.s_type = DONE;
        myMsg.s_header.s_local_time = 0L;

        logToFile(info->eventFd, myMsg.s_payload);

        send_multicast(info, &myMsg);
    }
    receiveAll(info);
    logToFile(info->eventFd, log_received_all_done_fmt, get_physical_time(), info->localID);
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
