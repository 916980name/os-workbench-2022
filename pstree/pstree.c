#define DIR_PROC "/proc/"
#define FILE_STAT "/stat"

#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>

int pidstr2int(char* str); 
typedef struct {
  int pid;
  char* name;
  int ppid;
} MyProcInfo;
// https://stackoverflow.com/questions/1675351/typedef-struct-vs-struct-definitions
typedef struct Node {
  MyProcInfo* this;
  struct Node* next;
  struct Node* child;
} Node;
MyProcInfo* processStat(char* filename);
MyProcInfo* readOneProcess(char* pid);
void get_process_info(const pid_t pid, MyProcInfo* info);
Node* list2tree(Node* listHead);
int add2tree(Node* treeHead, MyProcInfo* item);
void printTree(Node* treeHead, size_t prefix);
Node* initANode(MyProcInfo* info);
void handleArgs(int argc, char *argv[]);
char* version_msg = "***** Custom pstree *****\n -p, --show-pids:  show pid\n -n, --numeric-sort: asc sort\n -V --version: version info\n";
char v_short[] = "-V";
char v_long[] = "--version";
bool pidFlag = false;
char p_short[] = "-p";
char p_long[] = "--show-pids";
// Ofcourse I did not implment this feature.
bool ascFlag = false;
char n_short[] = "-n";
char n_long[] = "--numeric-sort";
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
    // printf("argv[%d] = %s\n", i, argv[i]);
  }
  assert(!argv[argc]);
  handleArgs(argc, argv);

  struct dirent* entry;
  DIR* procdir = opendir(DIR_PROC);
  if(procdir == NULL) {
    perror("opendir() error");
  }
  Node* listHead = malloc(sizeof(Node));
  Node* now = listHead;
  while ((entry = readdir(procdir)) != NULL) {
    MyProcInfo* info = readOneProcess(entry->d_name);
    if(info == NULL) continue;
    Node* n = initANode(info);
    now->next = n;
    now = n;
  }
  Node* treeHead = list2tree(listHead);
  printTree(treeHead, 0);
  closedir(procdir);
  return 0;
}

void handleArgs(int argc, char *argv[]) {
  for(int i = 0; i < argc; i++) {
    if(strcmp(v_short, argv[i]) == 0 || strcmp(v_long, argv[i]) == 0) {
      fputs(version_msg, stdout); 
      exit(0);
    }
    if(strcmp(n_short, argv[i]) == 0 || strcmp(n_long, argv[i]) == 0) {
      ascFlag = true;
      continue;
    }
    if(strcmp(p_short, argv[i]) == 0 || strcmp(p_long, argv[i]) == 0) {
      pidFlag = true;
      continue;
    }
  }
}

MyProcInfo* readOneProcess(char* pid) {
  // some of dir have 'stat' file but not the pid dir we want, 
  // so I have to determine whether it is a number.
  int pidDec = pidstr2int(pid);
  if(pidDec < 0) {
    return NULL;
  }
  // 'stat' file exists
  int length = sizeof(DIR_PROC) + sizeof(FILE_STAT) + sizeof(pid);
  char stat_s[length];
  sprintf(stat_s, DIR_PROC"%s"FILE_STAT, pid);
  // read 'stat'
  MyProcInfo* info = malloc(sizeof(MyProcInfo));
  get_process_info(pidDec, info);
  // printf("%d(%d): %s\n", info->pid, info->ppid, info->name);
  return info;
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
      char* name_s = malloc(sizeof(char) * (i - head) + 1);
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

Node* initANode(MyProcInfo* info) {
  Node* n = malloc(sizeof(Node));
  n->next = NULL;
  n->child = NULL;
  n->this = info;
  return n;
}

Node* list2tree(Node* listHead) {
  // These is no pid 0 process
  MyProcInfo* zero = malloc(sizeof(MyProcInfo));
  zero->pid = 0;
  Node* treeHead = initANode(zero);
  Node* p = listHead->next;
  while(p != NULL) {
    add2tree(treeHead, p->this);
    p = p->next;
  }
  return treeHead;
}

int add2tree(Node* treeHead, MyProcInfo* item) {
  if(treeHead == NULL || treeHead->this == NULL) return -1;
  if(treeHead->this->pid == item->ppid) {
    if(treeHead->child == NULL) {
      treeHead->child = initANode(item);
      return 0;
    } else {
      Node* p = treeHead->child;
      while(p->next != NULL) {
        p = p->next;
      }
      p->next = initANode(item);
      return 0;
    }
  } else {
    if(add2tree(treeHead->child, item) == 0) return 0;
    if(add2tree(treeHead->next, item) == 0) return 0;
  }
}

// https://stackoverflow.com/questions/5770940/how-repeat-a-string-in-language-c
char* str_repeat(char* str, size_t times) {
    if (times < 1) return "";
    char *ret = malloc(sizeof(str) * times + 1);
    if (ret == NULL) return "";
    strcpy(ret, str);
    while (--times > 0) {
        strcat(ret, str);
    }
    return ret;
}

void printTree(Node* treeHead, size_t prefix) {
  if(treeHead == NULL) return;
  MyProcInfo* info = treeHead->this;
  if(info == NULL) return;

  char* pid_s = "";
  int pid_len = 0;
  if(pidFlag) {
    pid_len = snprintf( NULL, 0, "%d", info->pid );
    pid_s = malloc( pid_len + 1 );
    snprintf( pid_s, pid_len + 1, "%d", info->pid );
  }
  int name_len = 0;
  if(info->name != NULL) {
    name_len = strlen(info->name);
  }
  size_t len = pid_len + 2 + name_len + 1;

  char* space = " ";
  char* prefix_space = str_repeat(space, prefix); // len: prefix
  char line[3] = "|--";

  char* buffer = malloc(prefix + 3 + len + 1);
	sprintf(buffer, "%s%s%s(%s)\n", prefix_space, line, pid_s, info->name);
  fputs(buffer, stdout);
  printTree(treeHead->child, prefix + 5);
  printTree(treeHead->next, prefix);
}