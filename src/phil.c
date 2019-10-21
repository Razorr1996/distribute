//
// Created by Artem Basalaev on 19.10.2019.
//

#include "phil.h"

void phil_init(LocalInfo *info, Phil **phil) {
    *phil = calloc(1, sizeof(Phil));
    Phil *phil2 = *phil;

    for (int i = 1; i <= info->nChild; ++i) {
        phil2->fork[i] = phil2->dirty[i] = phil2->reqf[i] = 0;
        if (i > info->localID) phil2->fork[i] = phil2->dirty[i] = 1;
        if (i < info->localID) phil2->reqf[i] = 1;
    }
}

int phil_check_forks(LocalInfo *info, Phil *phil) {
    for (int i = 1; i <= info->nChild; ++i) {
        if (i == info->localID) continue;
        if (!phil->fork[i]) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int phil_set_all_dirty(LocalInfo *info, Phil *phil) {
    for (int i = 1; i <= info->nChild; ++i) {
        if (i == info->localID) continue;
        phil->dirty[i] = 1;
    }
    return EXIT_SUCCESS;
}

void sprint_line(char *buf, int *a, int len, int start, int end) {
    int printed = 1;
    for (int i = 0; i < len; ++i) {
        printed += snprintf(buf + printed - 1,
                            MAX_MESSAGE_LEN - printed + 1,
                            " %s%d%s",
                            i == start ? "|" : "",
                            a[i],
                            i == end ? "|" : "");
    }
}

void phil_print(LocalInfo *info, Phil *phil) {
    char buf[MAX_MESSAGE_LEN];
    logToFile(info->eventFd, "    P[%d]: %d\n", info->localID, get_lamport_time());
    sprint_line(buf, phil->fork, MAX_PROCESS_ID + 1, 1, info->nChild);
    logToFile(info->eventFd, " fork[%d]: %s\n", info->localID, buf);
    sprint_line(buf, phil->dirty, MAX_PROCESS_ID + 1, 1, info->nChild);
    logToFile(info->eventFd, "dirty[%d]: %s\n", info->localID, buf);
    sprint_line(buf, phil->reqf, MAX_PROCESS_ID + 1, 1, info->nChild);
    logToFile(info->eventFd, " reqf[%d]: %s\n", info->localID, buf);
}
