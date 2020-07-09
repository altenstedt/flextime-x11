#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/extensions/scrnsaver.h>
#include <argp.h>

#include "config.h"
#include "measurement.pb-c.h"

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "Flextime -- tracking working hours";

static struct argp argp = { 0, 0, 0, doc };

#define PARENT_COUNT 3

static unsigned int interval = 60; // seconds, should be called measurement interval
static unsigned int flushInterval = PARENT_COUNT * 60; // seconds



Measurement measurements[PARENT_COUNT]; // the number of measurements within a flush
int idx = 0;

Measurements parent;
void *parent_buf;
unsigned parent_len;

void flush_measurements() {
  time_t now = time(NULL);

  char formatTimeBuffer[20];
  strftime(formatTimeBuffer, 20, "%FT%TZ", localtime(&now));

  char* home = getenv("HOME");

  char fileName[1024];
  snprintf(fileName, 1024, "%s/.flextime/data/%s.bin", home, formatTimeBuffer);
  FILE *file = fopen(fileName, "w+");

  fwrite(parent_buf, parent_len, 1, file);

  fclose(file);
}

void initialize_measurements() {
  for (int i = 0; i < PARENT_COUNT; i++) {
    Measurement msg = MEASUREMENT__INIT;

    parent_len = measurement__get_packed_size(&msg);

    parent_buf = malloc(parent_len);
    measurement__pack(&msg, parent_buf);

    measurements[i] = msg;
  }
}

void create_measurements(unsigned int interval, const char* zone, const Measurement measurements[], const int length) {
  Measurements msgs = MEASUREMENTS__INIT;

  msgs.interval = interval;

  size_t size = strlen(zone);
  msgs.zone = malloc(sizeof(char) * size);

  strcpy(msgs.zone, zone);
  
  msgs.n_measurements = length;
  msgs.measurements = malloc(sizeof(Measurement) * length);

  for (int i = 0; i < length; i++) {
    msgs.measurements[i] = malloc(sizeof(Measurement));
    measurement__init(msgs.measurements[i]);

    msgs.measurements[i]->kind = measurements[i].kind;
    msgs.measurements[i]->idle = measurements[i].idle;
    msgs.measurements[i]->timestamp = measurements[i].timestamp;
  }

  parent_len = measurements__get_packed_size(&msgs);

  parent_buf = malloc(parent_len);
  measurements__pack(&msgs, parent_buf);
  
  parent = msgs;
}

// https://stackoverflow.com/a/4702411
long get_idle_time() {
  time_t idle_time;
  static XScreenSaverInfo *mit_info;
  Display *display;
  int screen;

  mit_info = XScreenSaverAllocInfo();

  if((display=XOpenDisplay(NULL)) == NULL) {
    return(-1);
  }

  screen = DefaultScreen(display);

  XScreenSaverQueryInfo(display, RootWindow(display,screen), mit_info);

  idle_time = (mit_info->idle) / 1000;

  XFree(mit_info);
  XCloseDisplay(display);

  return idle_time;
}

int finishing = 0;

// sleep(3) can be interrupted by signals
void sleep_with_interrupt(unsigned int seconds) {
    unsigned int left = seconds;
    while ((left > 0) && (!finishing)) {
      left = sleep (left);
    }
}

void create_measurement_maybe() {
  time_t now = time(NULL);

  long idle = get_idle_time();

  // If the idle time is larger than the interval, we do not create a
  // measurement since there is nothing to add to the already created
  // measurements.  The time between measurement is not perfect, so be on
  // the safe side, we compared to a somewhat larger value.
  if (idle > interval * 1.5) {
    return;
  }

  measurements[idx].kind = MEASUREMENT__KIND__MEASUREMENT;
  measurements[idx].idle = idle;
  measurements[idx].timestamp = (intmax_t)now;

  idx++;

  if (idx == PARENT_COUNT) {
    char* zone = localtime(&now)->tm_zone;
    create_measurements(interval, zone, measurements, PARENT_COUNT);
    flush_measurements();
    idx = 0; // reset
  }
}

int main(int argc, char **argv)
{
  argp_parse (0, argc, argv, 0, 0, 0);
  char* home = getenv("HOME");

  char topFolderName[1024];
  snprintf(topFolderName, 1024, "%s/.flextime", home);

  char dataFolderName[1024];
  snprintf(dataFolderName, 1024, "%s/.flextime/data", home);

  if (0 == mkdir(topFolderName, 0777)) {
    mkdir(dataFolderName, 0777);
    printf("Created folder %s\n", dataFolderName);
  }

  time_t now = time(NULL);
  char* zone = localtime(&now)->tm_zone;

  initialize_measurements();

  while (1) {
    create_measurement_maybe();
    sleep_with_interrupt(interval);
  }

  return 0;
}
