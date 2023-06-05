#include <mutex>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/time.h>
#include <string>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "main.h"
#include "tools.h"

std::mutex log_mutex;
extern bool debugFlag;

void lprintf(const char *format, ...) {
  // Only print if debug flag is set, else do nothing
  if (debugFlag) {
        va_list ap;
        char fmt[2048];

        //actual time
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char buf[256];
        strcpy(buf, asctime(timeinfo));
        buf[strlen(buf)-1] = 0;

        //connect with args
        snprintf(fmt, sizeof(fmt), "%s", format);

        //put on screen:
        va_start(ap, format);
        vprintf(fmt, ap);
        va_end(ap);

        //to the logfile:
        static FILE *log;
        log_mutex.lock();
        log = fopen(LOG_FILE, "a");
        va_start(ap, format);
        vfprintf(log, fmt, ap);
        va_end(ap);
        fclose(log);
        log_mutex.unlock();
    }
}

int print_help() {
    printf("\nUSAGE:  ./smart_bms  [-h | --help], [-1 | --once], [-d | --debug]\n\n");

    printf("SUPPORTED ARGUMENTS:\n");
    printf("          -h | --help           This Help Message\n");
    printf("          -1 | --once           Runs one iteration on the BMS, and then exits\n");
    printf("          -d | --debug          Additional debugging\n\n");


    return 1;
}