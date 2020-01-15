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

#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "banking.h"

#include "functions.h"

void total_sum_snapshot() {

}

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
//    logToFile(pInfo->eventFd, "! %d--[%d]->%d\n", src, amount, dst);
    increment_vector_time();
    Message msg;
    memset(&msg, 0, sizeMessage);

    setMessage(&msg, TRANSFER, 0);

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

int updateBalance(BalanceHistory *history, TransferOrder *order) {
    BalanceState *new_state = &history->s_history[history->s_history_len];
    memset(new_state, 0, sizeof(BalanceState));
    if (history->s_history_len > 0) {
        BalanceState *old_state = &history->s_history[history->s_history_len - 1];
        new_state->s_balance = old_state->s_balance;
    } else {
        new_state->s_balance = 0;
    }

    new_state->s_time = get_vector_time();
    for (int i = 0; i < pInfo->nChild; ++i) {
        new_state->s_timevector[i] = pInfo->timeVector[i];
    }

    if (order->s_dst == history->s_id) {
        new_state->s_balance += order->s_amount;
    } else {
        new_state->s_balance -= order->s_amount;
    }

    history->s_history_len++;
    return EXIT_SUCCESS;
}

void childStartedMsg(LocalInfo *info, Message *msg, BalanceHistory *balance) {
    setMessage(msg, STARTED, 0);
    logToFile(info->eventFd, "process %d, time in start %d\n", info->localID, get_vector_time());
    BalanceState state = balance->s_history[0];
    snprintf(msg->s_payload, MAX_PAYLOAD_LEN, log_started_fmt, get_vector_time(), info->localID, info->pid,
             info->pPid, state.s_balance);
    msg->s_header.s_payload_len = (uint16_t) strlen(msg->s_payload);
}

int childLoop(LocalInfo *info, BalanceHistory *balance) {
    Message msg;
    TransferOrder order;
    int res;
    memset(&msg, 0, sizeof msg);
    memset(&order, 0, sizeof order);
    while (1) {
        res = receive_any(info, &msg);
        if (res != EXIT_SUCCESS) return res;
        switch (msg.s_header.s_type) {
            //TODO
            case TRANSFER: {
                memcpy(&order, msg.s_payload, msg.s_header.s_payload_len);
                increment_vector_time();
                logToFile(pInfo->eventFd, "!P %d, T %d; %d--[%d]->%d; last %d\n", pInfo->localID, get_vector_time(),
                          order.s_src, order.s_amount, order.s_dst, pInfo->lastMsgPid);
                updateBalance(balance, &order);
                if (order.s_src == info->localID) {
                    //TODO
                    msg.s_header.s_local_time = get_vector_time();
                    logToFile(info->eventFd, log_transfer_out_fmt, get_vector_time(), info->localID,
                              order.s_amount, order.s_dst);
                    res = send(info, order.s_dst, &msg);
                    if (res != EXIT_SUCCESS) return res;
                } else {
                    setMessage(&msg, ACK, 0);
                    logToFile(info->eventFd, log_transfer_in_fmt, get_vector_time(), info->localID,
                              order.s_amount, order.s_src);
                    res = send(info, PARENT_ID, &msg);
                    if (res != EXIT_SUCCESS) return res;
                }
                break;
            }
            case STOP: {
                return EXIT_SUCCESS;
            }
            default:
                return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int child(LocalInfo *info, BalanceHistory *balance) {
    Message msg;
    closeUnnecessaryPipes(info);
    increment_vector_time();
    {
        childStartedMsg(info, &msg, balance);
        logToFile(info->eventFd, msg.s_payload);
        send_multicast(info, &msg);
    }
    receiveAll(info);

    logToFile(info->eventFd, log_received_all_started_fmt, get_vector_time(), info->localID);
    childLoop(info, balance);
    increment_vector_time();
    {
        setMessage(&msg, DONE, 0);
        snprintf(msg.s_payload, MAX_PAYLOAD_LEN, log_done_fmt, get_vector_time(),
                 info->localID,
                 balance->s_history[balance->s_history_len - 1].s_balance);
        msg.s_header.s_payload_len = (uint16_t) strlen(msg.s_payload);
        logToFile(info->eventFd, msg.s_payload);

        send_multicast(info, &msg);
    }
    receiveAll(info);
    logToFile(info->eventFd, log_received_all_done_fmt, get_vector_time(), info->localID);

    closeUsedPipes(info);
    close(info->eventFd);
    return EXIT_SUCCESS;
}

int parent(LocalInfo *info) {
    Message msg;
    closeUnnecessaryPipes(info);

    receiveAll(info);

    logToFile(info->eventFd, log_received_all_started_fmt, get_vector_time(), info->localID);
    //Parent work
    bank_robbery(info, info->nChild);
    increment_vector_time();
    setMessage(&msg, STOP, 0);
    send_multicast(info, &msg);

    receiveAll(info);
    //End Parent work
    closeUsedPipes(info);
    for (int j = 0; j < info->nChild; ++j) {
        wait(NULL);
    }
    logToFile(info->eventFd, "All children end\n");
    close(info->eventFd);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    LocalInfo *info = malloc(sizeof(LocalInfo));
    memset(info, 0, sizeof(LocalInfo));
    pInfo = info;
    info->lab = 3;
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

    BalanceHistory balances[info->nChild + 1];
    TransferOrder order;
    memset(&order, 0, sizeof(TransferOrder));
    for (int i = 1; i < info->nChild + 1; i++) {
        memset(&balances[i], 0, sizeof(BalanceHistory));
        balances[i].s_id = i;
        order.s_dst = i;
        order.s_amount = atoi(argv[optind + i - 1]);
        printf("%d:%d\n", i, order.s_amount);
        balances[i].s_history_len = 0;
        updateBalance(&balances[i], &order);
    }

    preFork(info);

    if (info->localID != PARENT_ID) {
        child(info, &balances[info->localID]);
    } else {
        parent(info);
    }
    return EXIT_SUCCESS;
}
