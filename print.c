#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include<sys/dir.h>
#include <unistd.h>
#include <X11/extensions/scrnsaver.h>
#include <argp.h>
#include <signal.h>
#include <time.h>

#include "config.h"
#include "measurement.pb-c.h"

#define MAX_MSG_SIZE 1024
#define MAX_PATH 4096 // https://stackoverflow.com/a/9449307

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "Flextime -- tracking working hours";

static struct argp argp = { 0, 0, 0, doc };

static size_t
read_buffer (unsigned max_length, uint8_t *out, FILE* file)
{
  size_t cur_len = 0;
  size_t nread;
  while ((nread=fread(out + cur_len, 1, max_length - cur_len, file)) != 0)
  {
    cur_len += nread;
    if (cur_len == max_length)
    {
      fprintf(stderr, "Maximum message length exceeded.\n");
      exit(1);
    }
  }
  return cur_len;
}

static unsigned int *timestamps = NULL;
static unsigned int ntimestamps = 0;

void read_file(FILE* file) {
  Measurements *msgs;

  uint8_t buf[MAX_MSG_SIZE];
  size_t msg_len = read_buffer(MAX_MSG_SIZE, buf, file);

  msgs = measurements__unpack(NULL, msg_len, buf);	
  if (msgs == NULL)
  {
    fprintf(stderr, "error unpacking incoming message\n");
    exit(1);
  }

  timestamps = realloc(timestamps, (ntimestamps + msgs->n_measurements) * sizeof(unsigned int));

  for (int i = 0; i < msgs->n_measurements; i++) {
    Measurement msg = *msgs->measurements[i];

    time_t t = (time_t)msg.timestamp;
    unsigned int idle = msg.idle;

    timestamps[ntimestamps + i] = t - idle;
  }

  ntimestamps += msgs->n_measurements;

  measurements__free_unpacked(msgs, NULL);
}

// https://stackoverflow.com/a/744822
int endsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// https://stackoverflow.com/a/6898456
pid_t proc_find(const char* name) 
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];

    if (!(dir = opendir("/proc"))) {
        perror("can't open /proc");
        return -1;
    }

    while((ent = readdir(dir)) != NULL) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
        FILE* fp = fopen(buf, "r");

        if (fp) {
            if (fgets(buf, sizeof(buf), fp) != NULL) {
                /* check the first token in the file, the program name */
                char* first = strtok(buf, " ");
                if (endsWith(first, name)) {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t)lpid;
                }
            }
            fclose(fp);
        }

    }

    closedir(dir);
    return -1;
}

void parseTime(unsigned int seconds, unsigned int *hours, unsigned int *minutes) {
  *hours = (seconds - (seconds % 3600)) / 3600;
  *minutes = ((seconds - *hours * 60 * 60) - (seconds % 60)) / 60;
}

void print_day(time_t start_time, time_t stop_time, unsigned int work) {
  char date[9];
  strftime(date, 9, "%Y%m%d", localtime(&start_time));

  char start[9];
  strftime(start, 9, "%H:%M:%S", localtime(&start_time));

  char stop[9];
  strftime(stop, 9, "%H:%M:%S", localtime(&stop_time));

  char day[14];
  strftime(day, 14, "%A", localtime(&start_time));

  char week[3];
  strftime(week, 3, "%V", localtime(&start_time));

  unsigned int seconds = stop_time - start_time;
  unsigned int hours;
  unsigned int minutes;
  unsigned int work_hours;
  unsigned int work_minutes;

  parseTime(seconds, &hours, &minutes);
  parseTime(work, &work_hours, &work_minutes);

  printf("%s %s — %s %2d:%02d (%2d:%02d) w/%2s %s\n", date, start, stop, hours, minutes, work_hours, work_minutes, week, day);
}

void print_timestamps() {
  if (ntimestamps == 0) {
    return;
  }

  time_t t = (time_t)timestamps[0];
  time_t start_time = (time_t)timestamps[0];
  time_t stop_time = (time_t)timestamps[0];
  unsigned int work = 0;

  char date[9];
  strftime(date, 9, "%Y%m%d", localtime(&t));

  for (int i = 0; i < ntimestamps; i++) {

    t = (time_t)timestamps[i];

    char current[9];
    strftime(current, 9, "%Y%m%d", localtime(&t));

    if (strcmp(current, date) != 0) {
      print_day(start_time, stop_time, work);

      start_time = (time_t)timestamps[i];
      stop_time = (time_t)timestamps[i];
      work = 0;

      strcpy(date, current);
    } else {
      // Same day
      stop_time = (time_t)timestamps[i];

      unsigned int fixed = 10 * 60;// 10 minutes

      if (stop_time - start_time < fixed) {
        work += stop_time - start_time;
      }

      if (i == ntimestamps - 1) {
        // Last measurement
        print_day(start_time, stop_time, work);
      }
    }
  }  
}

int intcmp (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

int main(int argc, char **argv)
{
  argp_parse (0, argc, argv, 0, 0, 0);
  char* home = getenv("HOME");

  char dataFolderName[1024];
  snprintf(dataFolderName, 1024, "%s/.flextime/data", home);

  struct stat sb;

  if (stat(dataFolderName, &sb) != 0) {
    printf("Folder %s does not exist.\n", dataFolderName);

    return 1;
  }

  pid_t pid = proc_find("flextimed");

  if (pid >= 0  ) {
    kill(pid, SIGUSR1);

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 200 * 1000; // nano seconds

    // Wait just a little while for the daemon to flush
    nanosleep(&ts, NULL);
  }

  FILE* fd;
  struct dirent* file;

  char path[MAX_PATH];

  struct dirent **namelist;
  int n = scandir(dataFolderName, &namelist, 0, alphasort);
  if (n < 0) {
    perror("scandir");
  }

  while(n--) {
    char *dot = strrchr(namelist[n]->d_name, '.');

    if (dot == NULL || strcmp(dot, ".bin")) {
      continue;
    }

    snprintf(path, MAX_PATH, "%s/%s", dataFolderName, namelist[n]->d_name);

    fd = fopen(path, "r");

    read_file(fd);

    fclose(fd);
  }

  qsort(timestamps, ntimestamps, sizeof(unsigned int), intcmp);

  print_timestamps();

  free(namelist);
  free(timestamps);

  return 0;
}
