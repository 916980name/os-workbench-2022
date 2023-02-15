#define DIR_PROC "/proc/"
#define FILE_STAT "/stat"

#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

const char LINE = '-';
const char TREE = '|';
const int TAB = 4;
int printOneProcess(char* pid, int tab);
int pidstr2int(char* str); 
typedef struct {
  int pid;
  char* name;
  char* stat;
  int ppid;
} MyProcInfo;
MyProcInfo* processStat(char* filename);
/**
 * Based on Linux 5.15.0-56-generic, Ubuntu 22.04
 * /proc/[pid], for pid
 * /proc/[pid]/comm, for proc name
 * /proc/[pid]/stat, get everything here.
 * [Unaviable in multi-thread] /proc/[pid]/task/[tid]/children, for children pid list
 */
int main(int argc, char *argv[]) {
  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
    printf("argv[%d] = %s\n", i, argv[i]);
  }
  assert(!argv[argc]);

  struct dirent* entry;
  DIR* procdir = opendir(DIR_PROC);
  if(procdir == NULL) {
    perror("opendir() error");
  }
  while ((entry = readdir(procdir)) != NULL) {
    printOneProcess(entry->d_name, 4);
  }
  closedir(procdir);
  return 0;
}

int printOneProcess(char* pid, int tab) {
  // some of dir have 'stat' file but not the pid dir we want, 
  // so I have to determine whether it is a number.
  int pidDec = pidstr2int(pid);
  if(pidDec < 0) {
    return -2;
  }
  // 'stat' file exists
  int length = sizeof(DIR_PROC) + sizeof(FILE_STAT) + sizeof(pid);
  char stat_s[length];
  sprintf(stat_s, DIR_PROC"%s"FILE_STAT, pid);
  /*
  int fd = open(stat_s, O_RDONLY);
  if(fd == -1) {
    return -1;
  }
  */
  // read 'stat'
  MyProcInfo* info = processStat(stat_s);
  printf("%d(%d): %s\n", info->pid, info->ppid, info->name);
  return 0;
}

/**
 * from: man strtol
 */
int pidstr2int(char* str) {
  char *endptr;
  long val;
  errno = 0;    
  val = strtol(str, &endptr, 0);
  if (errno != 0) {
      return -1;
  }
  if (endptr == str) {
      return -2;
  }
  if (*endptr != '\0')
      return -3;
  return val;
}

MyProcInfo* processStat(char* filename) {
  FILE* thefile = fopen(filename, "r");
  if(thefile == NULL) {
    return NULL;
  }
  MyProcInfo* info = malloc(sizeof(MyProcInfo));
  fscanf(thefile, "%d (%*[^)]%*[)] %c %d ", &info->pid, info->name, info->stat, &info->ppid);
  return info;
}