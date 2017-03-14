#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <time.h>
#include <getopt.h>
#include <sys/types.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

#define MAX_FORK_MAGIC 18

local_id localID = 0;
int nChild = 8;

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

int main(int argc, char *argv[]) {
    int opt, i;
    int fDes[2];
    pid_t pid = 1;
    FILE *eventFile = fopen(events_log, "a");
    if (eventFile == NULL) {
        fprintf(stderr, "Cannot open events file.\n");
        exit(EXIT_FAILURE);
    }
    opt = getopt(argc, argv, "p:");
    if (opt != -1 && optarg != NULL) {
        sscanf(optarg, "%d", &nChild);
        if (nChild > MAX_FORK_MAGIC) nChild = MAX_FORK_MAGIC;
        if (nChild > MAX_PROCESS_ID) nChild = MAX_PROCESS_ID;
    }
    int readPipe[nChild + 1][nChild + 1];
    int writePipe[nChild + 1][nChild + 1];
    {//debug output
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        logToFile(eventFile, "\nTime: %s", asctime(timeinfo));
        logToFile(eventFile, "nChild=%d\n", nChild);
        logToFile(eventFile, "argc=%d\n", argc);
        for (i = 0; i < argc; ++i) {
            logToFile(eventFile, "argv[%d]=%s\n", i, argv[i]);
        }
    }
    memset(readPipe, -1, sizeof(readPipe));
    memset(writePipe, -1, sizeof(writePipe));

    for (i = 0; i <= nChild; ++i) {
        for (int j = 0; j <= nChild; ++j) {
            if (i != j) {
                if (pipe(fDes)) {
                    fprintf(stderr, "Cannot open pipe.\n");
                    exit(EXIT_FAILURE);
                } else {
                    readPipe[i][j] = fDes[0];
                    writePipe[i][j] = fDes[1];
                }
            }
        }
    }

    for (i = 1; i <= nChild && pid != 0; ++i) {
        pid = fork();
        if (pid == 0) {
            localID = (local_id) i;
        }

    }
    logToFile(eventFile, log_started_fmt, localID, getpid(), getppid());
    //
    fclose(eventFile);
    return EXIT_SUCCESS;
}
