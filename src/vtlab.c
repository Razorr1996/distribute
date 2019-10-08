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

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    increment_lamport_time();
    Message msg;
    memset(&msg, 0, sizeMessage);

    msg.s_header.s_type = TRANSFER;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_local_time = get_lamport_time();

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

int updateBalance(BalanceHistory *history, TransferOrder *order, timestamp_t sendTime) {
    timestamp_t nowTime = get_lamport_time();
    if (nowTime == 0) memset(&history->s_history[0], 0, sizeof(history->s_history[0]));
    history->s_history[history->s_history_len].s_balance_pending_in = 0;
    if (nowTime > 0)
        for (timestamp_t time = history->s_history_len; time <= nowTime; time++) {
            history->s_history[time] = history->s_history[time - 1];
            history->s_history[time].s_time++;
        }
    if (order->s_dst == history->s_id)
        for (timestamp_t time = sendTime; time < nowTime; ++time) {
//            history->s_history[time].s_balance_pending_in = order->s_dst == history->s_id ? order->s_amount : 0;
            history->s_history[time].s_balance_pending_in = order->s_amount;
        }
    history->s_history[nowTime].s_balance += (order->s_dst == history->s_id ? 1 : -1) * order->s_amount;
    history->s_history[nowTime].s_balance_pending_in = 0;
    history->s_history_len = (uint8_t) (nowTime + 1);
    return EXIT_SUCCESS;
}

void childStartedMsg(LocalInfo *info, Message *myMsg, BalanceHistory *data) {
    memset(myMsg, 0, sizeMessage);
    BalanceState state = data[info->localID].s_history[get_lamport_time()];
    myMsg->s_header.s_magic = MESSAGE_MAGIC;
    myMsg->s_header.s_type = STARTED;
    myMsg->s_header.s_local_time = get_lamport_time();
    snprintf(myMsg->s_payload, MAX_PAYLOAD_LEN, log_started_fmt, get_lamport_time(), info->localID, info->pid,
             info->pPid, state.s_balance);
    myMsg->s_header.s_payload_len = (uint16_t) strlen(myMsg->s_payload);
}

int childLoop(LocalInfo *info, BalanceHistory *data) {
    Message msg;
    TransferOrder order;
    BalanceHistory *balance = &data[info->localID];
    int res;
    memset(&msg, 0, sizeof msg);
    memset(&order, 0, sizeof order);
    while (1) {
        res = receive_any(info, &msg);
        if (res != EXIT_SUCCESS) return res;
        set_lamport_time(msg.s_header.s_local_time);
        increment_lamport_time();
        switch (msg.s_header.s_type) {
            case TRANSFER: {
                memcpy(&order, msg.s_payload, msg.s_header.s_payload_len);
                increment_lamport_time();
                updateBalance(balance, &order, msg.s_header.s_local_time);
                if (order.s_src == info->localID) {
                    msg.s_header.s_local_time = get_lamport_time();
                    logToFile(info->eventFd, log_transfer_out_fmt, get_lamport_time(), info->localID,
                              order.s_amount, order.s_dst);
                    res = send(info, order.s_dst, &msg);
                    if (res != EXIT_SUCCESS) return res;
                } else {
                    msg.s_header.s_type = ACK;
                    msg.s_header.s_payload_len = 0;
                    msg.s_header.s_local_time = get_lamport_time();
                    logToFile(info->eventFd, log_transfer_in_fmt, get_lamport_time(), info->localID,
                              order.s_amount, order.s_src);
                    res = send(info, PARENT_ID, &msg);
                    if (res != EXIT_SUCCESS) return res;
                }
                break;
            }
            case STOP: {
                order.s_dst = 0;
                order.s_src = info->localID;
                order.s_amount = 0;
                updateBalance(balance, &order, get_lamport_time());
                return EXIT_SUCCESS;
            }
            default:
                return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int child(LocalInfo *info, BalanceHistory *data) {
    Message msg;
    closeUnnecessaryPipes(info);
    increment_lamport_time();
    {
        childStartedMsg(info, &msg, data);
        logToFile(info->eventFd, msg.s_payload);
        send_multicast(info, &msg);
    }
    receiveAll(info);

    logToFile(info->eventFd, log_received_all_started_fmt, get_lamport_time(), info->localID);
    childLoop(info, data);
    increment_lamport_time();
    {
        memset(&msg, 0, sizeMessage);
        snprintf(msg.s_payload, MAX_PAYLOAD_LEN, log_done_fmt, get_lamport_time(),
                 info->localID,
                 data[info->localID].s_history[get_lamport_time()].s_balance);
        msg.s_header.s_magic = MESSAGE_MAGIC;
        msg.s_header.s_payload_len = (uint16_t) strlen(msg.s_payload);
        msg.s_header.s_type = DONE;
        msg.s_header.s_local_time = get_lamport_time();

        logToFile(info->eventFd, msg.s_payload);

        send_multicast(info, &msg);
    }
    receiveAll(info);
    logToFile(info->eventFd, log_received_all_done_fmt, get_lamport_time(), info->localID);
    increment_lamport_time();

    BalanceHistory *balance = &data[info->localID];
    TransferOrder order;
    order.s_src = 0;
    order.s_dst = info->localID;
    order.s_amount = 0;
    updateBalance(balance, &order, get_lamport_time());

    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = BALANCE_HISTORY;
    msg.s_header.s_local_time = get_lamport_time();
    msg.s_header.s_payload_len = sizeof(BalanceHistory) - sizeof(balance->s_history) +
                                 balance->s_history_len * sizeof(BalanceState);
    memcpy(msg.s_payload, balance, msg.s_header.s_payload_len);
    send(info, PARENT_ID, &msg);
    closeUsedPipes(info);
    close(info->eventFd);
    return EXIT_SUCCESS;
}

int parent(LocalInfo *info) {
    Message msg;
    closeUnnecessaryPipes(info);

    receiveAll(info);

    logToFile(info->eventFd, log_received_all_started_fmt, get_lamport_time(), info->localID);
    //Parent work
    bank_robbery(info, info->nChild);
    increment_lamport_time();
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = STOP;
    msg.s_header.s_local_time = get_lamport_time();
    send_multicast(info, &msg);

    logToFile(info->eventFd, "0\n");
    receiveAll(info);
    logToFile(info->eventFd, "1\n");

    AllHistory all;
    all.s_history_len = (uint8_t) info->nChild;
    logToFile(info->eventFd, "2.0\n");
    for (local_id i = 1; i <= info->nChild; ++i) {
        logToFile(info->eventFd, "3.0\n");
        if (receive(info, i, &msg) != EXIT_SUCCESS) return EXIT_FAILURE;
        set_lamport_time(msg.s_header.s_local_time);
        increment_lamport_time();
        logToFile(info->eventFd, "3.1\n");
        memcpy(&all.s_history[i - 1], msg.s_payload, msg.s_header.s_payload_len);
        logToFile(info->eventFd, "3.2\n");
    }
    logToFile(info->eventFd, "4.0\n");
    print_history(&all);
    logToFile(info->eventFd, "5.0\n");
    //End Parent work
    closeUsedPipes(info);
    logToFile(info->eventFd, "6.0\n");
    for (int j = 0; j < info->nChild; ++j) {
        logToFile(info->eventFd, "7.0\n");
        wait(NULL);
        logToFile(info->eventFd, "7.1\n");
    }
    logToFile(info->eventFd, "All children end\n");
    close(info->eventFd);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    LocalInfo *info = malloc(sizeof(LocalInfo));
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
        balances[i].s_id = i;
        order.s_dst = i;
        order.s_amount = atoi(argv[optind + i - 1]);
        printf("%d:%d\n", i, order.s_amount);
        balances[i].s_history_len = 0;
        updateBalance(&balances[i], &order, 0);
    }

    preFork(info);

    if (info->localID != PARENT_ID) {
        child(info, balances);
    } else {
        parent(info);
    }
    return EXIT_SUCCESS;
}
