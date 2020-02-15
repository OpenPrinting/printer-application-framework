#include "server.h"
#include <sys/socket.h>

void initialize() {
  char filename[PATH_MAX];
  snprintf(filename, PATH_MAX - 1, "%s/config/framework.config", tmpdir);
  cups_file_t* config = cupsFileOpen(filename, "r");
  if (config == NULL) {
    fprintf(stderr, "Unable to open configuration file!\n");
    return;
  }
  char line[2048];
  int maxLines = 20;
  char lines[maxLines][2][1024];
  int numLines = 0;
  while (cupsFileGets(config, line, sizeof(line))) {
    int len = strlen(line);
    int comment = 0;
    for(int i = 0; i < len; i++) {
      if (isalnum(line[i]))
	break;
      if (line[i] == '#') {
        comment = 1;
        break;
      }
    }
    /*fprintf(stderr, "Line: %s %d\n", line, comment);*/
    if (comment)
      continue;
    char tokens[2][1024];
    char temp[1024];
    int index=0;
    int numTokens=0;
    for (int i = 0; i <= len; i++) {
      if (isalpha(line[i]) && index < 1024)
        temp[index++] = line[i];
      else {
        if (index) {
          temp[index] = '\0';
          /*fprintf(stderr, "token: %s %d\n", temp, numTokens);*/
          strcpy(tokens[numTokens++], temp);
          index = 0;
          if (numTokens == 2)
	    break;
        }
      }
    }
    if (numTokens == 2) {
      strcpy(lines[numLines][0], tokens[0]);
      strcpy(lines[numLines][1], tokens[1]);
      numLines++;
      if(numLines >= maxLines)
        break;
    }
  }
  cupsFileClose(config);
  for (int i = 0; i < numLines; i++) {
    /*fprintf(stderr, "Lines: %d %s %s\n", i, lines[i][0], lines[i][1]);*/
    if (!strcmp(lines[i][0], "DebuggingLevel")) {
      char level[2] = "1";
      if (!strncasecmp(lines[i][1], "DEBUG", 5))
        level[0] = '2';
      else if(!strncasecmp(lines[i][1],"DEBUG2",6))
        level[0] = '3';
      setenv("DEBUG_LEVEL", level, 1);
    }
  }
}

/*
 * main() -
 */

int main(int argc, char* argv[]) {
  pid_t pid, ppid;

  ppid = getpid();

  if (getenv("SNAP"))
    snap = strdup(getenv("SNAP"));
  else
    snap = strdup("");

  if (getenv("SNAP_COMMON"))
    tmpdir = strdup(getenv("SNAP_COMMON"));
  else
    tmpdir = strdup("/var/tmp");

  initialize();
  
  con_devices = cupsArrayNew((cups_array_func_t)compare_devices, NULL);
  temp_devices = cupsArrayNew((cups_array_func_t)compare_devices, NULL);

  pending_signals[0] = 0;
  for (int i = 1; i <= 2 * NUM_SIGNALS; i++)
    pending_signals[i] = 1;
  
  if (pthread_mutex_init(&signal_lock, NULL) != 0) {
    printf("ERROR: Mutex init Failed\n");
    return -1;
  }
  
  pthread_create(&hardwareThread, NULL, start_hardware_monitor, NULL);
#if HAVE_AVAHI
  pthread_create(&avahiThread, NULL, start_avahi_monitor, NULL);
#endif

  kill_listeners();

  while (1) {            /*Infinite loop*/
    sleep(10);
    for (int i = 1; i <= 2 * NUM_SIGNALS; i++) {
      int exec = 0;
      pthread_mutex_lock(&signal_lock);
      if (pending_signals[i]) {
        pending_signals[i] = 0;
        exec = 1;
      }
      pthread_mutex_unlock(&signal_lock);
      if(exec)
        get_devices(i % 2, i);
    }
    get_devices(2, 0);
  }
  cleanup();
  
  return 0;
}
