#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/extensions/scrnsaver.h>
#include <argp.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>

#include "config.h"
#include "measurement.pb-c.h"

#define PARENT_COUNT 15

static unsigned int foreground = 0;
static unsigned int verbose = 0;
static unsigned int interval = 60; // seconds, should be called measurement interval

Measurement measurements[PARENT_COUNT]; // the number of measurements within a flush
int idx = 0;

Measurements parent;
void *parent_buf;
unsigned parent_len;

char pipeFileName[1032];

void create_measurements(unsigned int interval, const char* zone) {
  Measurements msgs = MEASUREMENTS__INIT;

  msgs.interval = interval;

  size_t size = strlen(zone);
  msgs.zone = malloc(sizeof(char) * size + 1);

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

void internal_log(int pri, const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    if (foreground) {
      vprintf(fmt, vl);
      printf("\n");
      fflush(stdout);
    } else {
      vsyslog(pri, fmt, vl);
    }

    va_end(vl);
}

void flush_pipe() {
  if (verbose) internal_log(LOG_DEBUG, "Writing message to pipe.");

  // Print a message on the named pipe
  int pipe = open(pipeFileName, O_RDWR); // Not O_WRONLY or we will hang if no reader
  write(pipe, "FLUSHED\n", 8);
  close(pipe);
}

void flush_measurements(int pipe) {
  if (idx == 0) {
    if (pipe) flush_pipe();

    if (verbose) internal_log(LOG_DEBUG, "No measurements to flush.");

    return; // Nothing to flush
  }

  if (verbose) internal_log(LOG_DEBUG, "Flushing measurements at index %d.", idx);

  const time_t now = time(NULL);
  const struct tm *local_now = localtime(&now);

  char formatTimeBuffer[20];
  strftime(formatTimeBuffer, 20, "%FT%TZ", local_now);

  char* home = getenv("HOME");

  char fileName[1024];
  snprintf(fileName, 1024, "%s/.flextime/data/%s.bin", home, formatTimeBuffer);

  if(access(fileName, F_OK ) != -1) {
    // The file already exists.  Since the name of the file is the time in
    // ISO8601 format resolved to seconds, it means that the data is flushed
    // already.
    if (idx == 1) {
      // This can happen, 1/60 of the times we flush within the same second
      internal_log(LOG_WARNING, "One measurement is discarded for file %s.", fileName);
    }

    if (idx > 1) {
      // This can never happen with a measurement interval of one minute
      internal_log(LOG_ERR, "%d measurements are discarded for file %s.", idx, fileName);
    }

    if (idx == 0) {
      // This should never happen
      internal_log(LOG_WARNING, "Zero measurements are discarded for file %s.", fileName);
    }

    return;
  }

  const char* zone = local_now->tm_zone;
  create_measurements(interval, zone);

  FILE *file = fopen(fileName, "w+");

  fwrite(parent_buf, parent_len, 1, file);

  fclose(file);
  if (pipe) flush_pipe();

  if (verbose) internal_log(LOG_DEBUG, "Flushed measurements to file %s.", fileName);
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

  if (verbose) internal_log(LOG_DEBUG, "Created measurement; index now at %d (of %d).", idx, PARENT_COUNT);

  if (idx == PARENT_COUNT) {
    flush_measurements(0);
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
    if (verbose) internal_log(LOG_DEBUG, "Did not create measurement since idle is too large (%d compared to %f).", idle, interval * 1.5);

    return;
  }

  create_measurement(MEASUREMENT__KIND__MEASUREMENT, idle);
}

void sighandler(int signum) {
  // In general, our approach to flushing data in a signal handler is
  // wrong.  If the main program is in the process of writing data to
  // files, then since the signal handler is also writing to files, the
  // state of the allocated data buffers are simply wrong.
  // 
  // We need to fix that, and avoid using stdio functions from the signal
  // handler.  In fact, only functions listed in signal-safety(7) can be
  // used in a signal handler.
  // 
  // https://stackoverflow.com/a/55179208
  // https://man7.org/linux/man-pages/man7/signal-safety.7.html

   internal_log(LOG_NOTICE, "Caught signal %d, flushing %d measurements.", signum, idx);

   int shutdown = signum != SIGUSR1;

   if (shutdown) {
    if (verbose) internal_log(LOG_DEBUG, "Shutdown measurement created.");
     create_measurement(MEASUREMENT__KIND__STOP, 0);
   }

   if (shutdown) 
    flush_measurements(0);
  else
    flush_measurements(1);

   idx = 0;

   if (shutdown) {
     closelog();

     internal_log (LOG_NOTICE, "Flextime daemon %s terminated.", PACKAGE_VERSION);

     _exit(0);
   }
}

// https://github.com/pasce/daemon-skeleton-linux-c
//
// This might all be wrong-headed, and we should not do it at all:
// https://unix.stackexchange.com/a/177361
static void skeleton_daemon()
{
    if (verbose) internal_log(LOG_DEBUG, "Forking a daemon process.");
    
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

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "Flextime -- tracking working hours";

/* A description of the arguments we accept. */
static char args_doc[] = "ARG1 ARG2";

/* The options we understand. */
static struct argp_option options[] = {
  {"foreground", 'f', 0, 0,  "Run in the foreground, not as a daemon" },
  {"verbose", 'v', 0, 0,  "Print verbose output" },
  { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
  char *args[2];                /* arg1 & arg2 */
  int foreground;
  int verbose;
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'f': 
      arguments->foreground = 1;
      break;

    case 'v': 
      arguments->verbose = 1;
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num >= 2)
        /* Too many arguments. */
        argp_usage (state);

      arguments->args[state->arg_num] = arg;

      break;

    case ARGP_KEY_END:
      if (state->arg_num < 0)
        /* Not enough arguments. */
        argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv)
{
  struct arguments arguments;

  /* Default values. */
  arguments.foreground = 0;
  arguments.verbose = 0;

  /* Parse our arguments; every option seen by parse_opt will
     be reflected in arguments. */
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  foreground = arguments.foreground;
  verbose = arguments.verbose;

  char* home = getenv("HOME");

  if (!foreground) {
    skeleton_daemon();
    internal_log(LOG_NOTICE, "Flextime daemon %s started.", PACKAGE_VERSION);
  } else {
    if (verbose) internal_log(LOG_DEBUG, "Running in the foreground.");
  }

  char topFolderName[1024];
  snprintf(topFolderName, 1024, "%s/.flextime", home);

  char dataFolderName[1024];
  snprintf(dataFolderName, 1024, "%s/.flextime/data", home);

  if (0 == mkdir(topFolderName, 0750)) {
    mkdir(dataFolderName, 0750);
    internal_log(LOG_NOTICE, "Created folder %s.", dataFolderName);
  }

  snprintf(pipeFileName, 1032, "%s/flushed", topFolderName);
  int pipeFileResult = mkfifo(pipeFileName, S_IRWXU | S_IRGRP);

  if (pipeFileResult != 0) {
    if (errno != EEXIST) {
        // Log a warning and keep going
        internal_log(LOG_WARNING, "Error %d when creating pipe %s (continuing).", errno, pipeFileName);
    }
  }

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

  if (!foreground) {
    internal_log(LOG_NOTICE, "Flextime daemon %s terminated (should never happen in main).", PACKAGE_VERSION);
    closelog();
  }

  return 1; // We never terminate from main
}
