#include <time.h>
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

static unsigned int interval = 60;

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

  Measurement msg = MEASUREMENT__INIT;
  void *buf;
  unsigned len;

  msg.kind = MEASUREMENT__KIND__MEASUREMENT;
  msg.idle = idle;
  msg.timestamp = (intmax_t)now;

  len = measurement__get_packed_size(&msg);

  buf = malloc(len);
  measurement__pack(&msg,buf);

  char formatTimeBuffer[20];
  strftime(formatTimeBuffer, 20, "%FT%TZ", localtime(&now));

  char* home = getenv("HOME");

  char fileName[1024];
  snprintf(fileName, 1024, "%s/.flextime/data/%s.bin", home, formatTimeBuffer);
  FILE *file = fopen(fileName, "w+");

  printf("Writing %d bytes of idle %ld to %s\n", len, idle, fileName);

  fwrite(buf, len, 1, file);

  fclose(file);

  free(buf);
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

  while (1) {
    create_measurement_maybe();
    sleep_with_interrupt(interval);
  }

  return 0;
}
