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

void read_file(FILE* file) {
  Measurement *msg;

  uint8_t buf[MAX_MSG_SIZE];
  size_t msg_len = read_buffer(MAX_MSG_SIZE, buf, file);

  msg = measurement__unpack(NULL, msg_len, buf);	
  if (msg == NULL)
  {
    fprintf(stderr, "error unpacking incoming message\n");
    exit(1);
  }

  time_t t = (time_t)msg->timestamp;

  char buffer[20];
  strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));

  printf("Parsed: %s %d %d\n", buffer, msg->idle, msg->kind);

  measurement__free_unpacked(msg, NULL);
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

  return 0;
}
