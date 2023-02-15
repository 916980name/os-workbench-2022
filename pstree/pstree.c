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
void get_process_info(const pid_t pid, MyProcInfo* info);
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
  // read 'stat'
  MyProcInfo* info = malloc(sizeof(MyProcInfo));
  get_process_info(pidDec, info);
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

/*
 *  [start, end)
 */
char* my_strncpy(char *dest, const char *src, int start, int end) {
    int i;
    for (i = 0; start < end && src[start] != '\0'; i++, start++) {
      dest[i] = src[start];
    }
    dest[i] = '\0';
    return dest;
}

/*
* 9496 ((sd-pam)) S 9495 9495 9495 0 -1 1077936448 52 0 0 0 0 0 0 0 20 0 1 0 153378 174600192 1271 18446744073709551615 1 1 0 0 0 0 0 4096 0 0 0 0 17 0 0 0 0 0 0 0 0 0 0 0 0 0 0
*/
void get_process_info(const pid_t pid, MyProcInfo* info) {
	char buffer[BUFSIZ];
	sprintf(buffer, DIR_PROC"%d"FILE_STAT, pid);
	FILE* fp = fopen(buffer, "r");
	if (fp) {
		size_t size = fread(buffer, sizeof (char), sizeof (buffer), fp);
		if (size > 0) {
      char pid_s[6];
      char ppid_s[6];
      char name_s[BUFSIZ];
      int i = 0;
      int head = 0;
      while(buffer[++i] != '\0') {
        if(buffer[i] == ' ') break;
      }
      my_strncpy(pid_s, buffer, 0, i);
			info->pid = pidstr2int(pid_s); // (1) pid  %d

      if(buffer[++i] != '(') {
        perror("Read stat format error\n");
      }
      head = ++i;

      /*
      * There is a assmuption that process name(maybe command) is complete parentheses 
      */ 
      int left_p_count = 1;
      while(buffer[i] != '\0') {
        if(buffer[i] == '(') left_p_count++;
        if(buffer[i] == ')') left_p_count--;
        if(left_p_count == 0) break;
        i++;
      }
      my_strncpy(name_s, buffer, head, i);
			info->name = name_s; // (2) comm  %s
      head = ++i;

      while(buffer[++i] != '\0') {
        if(buffer[i] == ' ') break;
      }
      head = ++i;
			
      while(buffer[++i] != '\0') {
        if(buffer[i] == ' ') break;
      }
      my_strncpy(ppid_s, buffer, head, i);
			info->ppid = pidstr2int(ppid_s); // (4) ppid  %d
		}
		fclose(fp);
	}
}