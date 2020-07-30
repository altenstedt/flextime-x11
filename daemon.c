#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/extensions/scrnsaver.h>
#include <argp.h>
#include <signal.h>
#include <syslog.h>

#include "config.h"
#include "measurement.pb-c.h"

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "Flextime -- tracking working hours";

static struct argp argp = { 0, 0, 0, doc };

#define PARENT_COUNT 15

static unsigned int interval = 60; // seconds, should be called measurement interval
static unsigned int flushInterval = PARENT_COUNT * 60; // seconds

Measurement measurements[PARENT_COUNT]; // the number of measurements within a flush
int idx = 0;

Measurements parent;
void *parent_buf;
unsigned parent_len;

void create_measurements(unsigned int interval, const char* zone) {
  Measurements msgs = MEASUREMENTS__INIT;

  msgs.interval = interval;

  size_t size = strlen(zone);
  msgs.zone = malloc(sizeof(char) * size);

  strcpy(msgs.zone, zone);
  
  msgs.n_measurements = idx;
  msgs.measurements = malloc(sizeof(Measurement) * idx);

  for (int i = 0; i < idx; i++) {
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

void flush_measurements() {
  if (idx == 0) {
    return; // Nothing to flush
  }

  time_t now = time(NULL);

  char formatTimeBuffer[20];
  strftime(formatTimeBuffer, 20, "%FT%TZ", localtime(&now));

  char* home = getenv("HOME");

  char fileName[1024];
  snprintf(fileName, 1024, "%s/.flextime/data/%s.bin", home, formatTimeBuffer);

  if(access(fileName, F_OK ) != -1) {
    // The file already exists.  Since the name of the file is the time in
    // ISO8601 format resolved to seconds, it means that the data is flushed
    // already.
    if (idx == 1) {
      // This can happen, 1/60 of the times we flush within the same second
      syslog(LOG_WARNING, "One measurement is discarded for file %s.", fileName);
    }

    if (idx > 1) {
      // This can never happen with a measurement interval of one minute
      syslog(LOG_ERR, "%d measurements are discarded for file %s.", idx, fileName);
    }

    return;
  }

  char* zone = localtime(&now)->tm_zone;
  create_measurements(interval, zone);

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

void create_measurement(Measurement__Kind kind, unsigned int idle) {
  time_t now = time(NULL);

  measurements[idx].kind = kind;
  measurements[idx].idle = idle;
  measurements[idx].timestamp = (intmax_t)now;

  idx++;

  if (idx == PARENT_COUNT) {
    flush_measurements();
    idx = 0;
  }
}

void create_measurement_maybe() {
  long idle = get_idle_time();

  // If the idle time is larger than the interval, we do not create a
  // measurement since there is nothing to add to the already created
  // measurements.  The time between measurement is not perfect, so be on
  // the safe side, we compared to a somewhat larger value.
  if (idle > interval * 1.5) {
    return;
  }

  create_measurement(MEASUREMENT__KIND__MEASUREMENT, idle);
}

void sighandler(int signum) {
   syslog(LOG_NOTICE, "Caught signal %d, flushing %d measurements.\n", signum, idx);

   int shutdown = signum != SIGUSR1;

   if (shutdown) {
     create_measurement(MEASUREMENT__KIND__STOP, 0);
   }

   flush_measurements();
   idx = 0;

   if (shutdown) {
     syslog (LOG_NOTICE, "Flextime daemon %s terminated.", PACKAGE_VERSION);
     closelog();
     exit(0);
   }
}

// https://github.com/pasce/daemon-skeleton-linux-c
static void skeleton_daemon()
{
    pid_t pid;
    
    /* Fork off the parent process */
    pid = fork();
    
    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);
    
     /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);
    
    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);
    
    /* Catch, ignore and handle signals */
    /*TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    
    /* Fork off for the second time*/
    pid = fork();
    
    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);
    
    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);
    
    /* Set new file permissions */
    umask(0);
    
    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");
    
    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
    
    /* Open the log file */
    openlog ("flextime", LOG_PID, LOG_DAEMON);
}

int main(int argc, char **argv)
{
  argp_parse (0, argc, argv, 0, 0, 0);
  char* home = getenv("HOME");

  skeleton_daemon();

  syslog (LOG_NOTICE, "Flextime daemon %s started.", PACKAGE_VERSION);

  char topFolderName[1024];
  snprintf(topFolderName, 1024, "%s/.flextime", home);

  char dataFolderName[1024];
  snprintf(dataFolderName, 1024, "%s/.flextime/data", home);

  if (0 == mkdir(topFolderName, 0777)) {
    mkdir(dataFolderName, 0777);
    syslog (LOG_NOTICE, "Created folder %s\n", dataFolderName);
  }

  time_t now = time(NULL);
  char* zone = localtime(&now)->tm_zone;

  signal(SIGINT, sighandler);
  signal(SIGHUP, sighandler);
  signal(SIGQUIT, sighandler);
  signal(SIGABRT, sighandler);
  signal(SIGTSTP, sighandler);
  signal(SIGTERM, sighandler);
  signal(SIGUSR1, sighandler);
  
  initialize_measurements();

  create_measurement(MEASUREMENT__KIND__START, 0);
  sleep_with_interrupt(interval);

  while (1) {
    create_measurement_maybe();
    sleep_with_interrupt(interval);
  }

  syslog (LOG_NOTICE, "Flextime daemon %s terminated (should never happen in main).", PACKAGE_VERSION);
  closelog();

  return 1; // We never terminate from main
}
