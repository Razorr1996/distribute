//
// Created by Artem Basalaev on 19.10.2019.
//

#ifndef DISTRIBUTED_PHIL_H
#define DISTRIBUTED_PHIL_H

#include <stdio.h>
#include <stdlib.h>

#include "banking.h"
#include "functions.h"
#include "ipc.h"

typedef struct Phil {
    int fork[MAX_PROCESS_ID + 1];
    int dirty[MAX_PROCESS_ID + 1];
    int reqf[MAX_PROCESS_ID + 1];
} Phil;

void phil_init(LocalInfo *info, Phil **phil);

int phil_check_forks(LocalInfo *info, Phil *phil);

int phil_set_all_dirty(LocalInfo *info, Phil *phil);

void phil_print(LocalInfo *info, Phil *phil);

#endif //DISTRIBUTED_PHIL_H
