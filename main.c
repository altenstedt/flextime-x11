#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/extensions/scrnsaver.h>

// https://stackoverflow.com/a/4702411
int get_idle_time() {
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

int main()
{
  printf("%d\n", get_idle_time());
  return 0;
}
