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

  long idle = get_idle_time();

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

  char fileName[1024];
  snprintf(fileName, 1024, "%s/.flextime/data/%s.bin", home, formatTimeBuffer);
  FILE *file = fopen(fileName, "w+");

  printf("Writing %d bytes of idle %ld to %s\n", len, idle, fileName);

  fwrite(buf, len, 1, file);

  fclose(file);

  free(buf);

  return 0;
}
