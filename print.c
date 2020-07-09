#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/extensions/scrnsaver.h>
#include <argp.h>

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


struct dates {
  char date[9];
  unsigned int start;
  unsigned int stop;
};

#define DATE_SIZE 3
struct dates dates[DATE_SIZE];
unsigned int dates_index = 0;

void add_or_update(Measurement measurement) {
  for (int i = 0; i < DATE_SIZE; i++) {
    time_t t = (time_t)measurement.timestamp;

    char date[9];
    strftime(date, 9, "%Y%m%d", localtime(&t));

    int index = -1;
    for (int j = 0; j < dates_index; j++) {
      if (strcmp(dates[j].date, date) == 0) {
        if (dates[j].start > measurement.timestamp) {
          dates[j].start = measurement.timestamp;
        }

        if (dates[j].stop < measurement.timestamp) {
          dates[j].stop = measurement.timestamp;
        }

        index = j;
        break;
      }
    }

    if (index == -1) {
      strcpy(dates[dates_index].date, date);
      dates[dates_index].start = measurement.timestamp;
      dates[dates_index].stop = measurement.timestamp;

      dates_index++;
    }
  }
}

void print_dates() {
  for (int i = 0; i < dates_index; i++) {

    time_t start_time = (time_t)dates[i].start;
    char start[9];
    strftime(start, 9, "%H:%M:%S", localtime(&start_time));

    time_t stop_time = (time_t)dates[i].stop;
    char stop[9];
    strftime(stop, 9, "%H:%M:%S", localtime(&stop_time));

    char day[14];
    strftime(day, 14, "%A", localtime(&start_time));

    char week[3];
    strftime(week, 3, "%V", localtime(&start_time));

    unsigned int seconds = dates[i].stop - dates[i].start;
    unsigned int hours = (seconds - (seconds % 3600)) / 3600;
    unsigned int minutes = ((seconds - hours * 60 * 60) - (seconds % 60)) / 60;
    printf("%s %s — %s %2d:%02d w/%2s %s\n", dates[i].date, start, stop, hours, minutes, week, day);
  }  
}

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

  for (int i = 0; i < msgs->n_measurements; i++) {
    Measurement msg = *msgs->measurements[i];

    time_t t = (time_t)msg.timestamp;

    char buffer[20];
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));

    add_or_update(msg);
  }

  measurements__free_unpacked(msgs, NULL);
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

  FILE* fd;
  DIR* dir;
  struct dirent* file;

  dir = opendir(dataFolderName);

  char path[MAX_PATH];

  while (file = readdir(dir))
  {
    char *dot = strrchr(file->d_name, '.');

    if (dot == NULL || strcmp(dot, ".bin")) {
      continue;
    }

    snprintf(path, MAX_PATH, "%s/%s", dataFolderName, file->d_name);

    fd = fopen(path, "r");

    read_file(fd);

    fclose(fd);
  }

  closedir(dir);

  print_dates();

  return 0;
}
