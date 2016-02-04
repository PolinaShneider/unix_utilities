#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>


#define DEBUG 0
#define DEBUG2 0
#define TEST_SLEEP 0

char **read_command(int *is_background, int *command_size) {

  char *x = NULL;
  char *t;
  *is_background = 0;
  // puts("Please enter a line of text");
  size_t bufsize=0;
  int size = 0;
  size = getline(&x, &bufsize, stdin);
  x[size-1] = '\0';
  int cnt = 0;
  int i=0;
  while (i<200 && x[i] != '\0' && (x[i] == ' ' || x[i] == '\t'))
    i++;
  if (i==0)
    i++;
  for (; i<200 && x[i] != '\0'; i++) {
    if ((x[i]==' ' || x[i] == '\t') && !(x[i-1]==' ' || x[i-1] == '\t'))
      cnt++;
  }
  cnt++;

  char **c = (char **)malloc((cnt+1)*sizeof(char **));

  i=0;
  t = strtok (x," \t");
  if (t == NULL)
    i--;
  else {
    c[i] = (char *)malloc((strlen(t))*sizeof(char));
    strcpy(c[i],t);
  }
  while (1)
  {
    t = strtok (NULL, " \t");
    if (t==NULL)
      break;
    else if (!strcmp(t, "&")) {
      *is_background = 1;
      if (DEBUG)
        printf("& detected\n");
      break;
    }
    else if (t[strlen(t)-1] == '&') {
      *is_background = 1;
      if (DEBUG)
        printf("& detected\n");
      t[strlen(t)-1] = '\0';
    }

    c[++i] = (char *)malloc((strlen(t))*sizeof(char));
    strcpy(c[i],t);
  }

  if (i==0 && c[0][strlen(c[0])-1] == '&') {
    *is_background = 1;
    if (DEBUG)
      printf("& detected\n");
    c[0][strlen(c[0])-1] = '\0';
  }

  c[++i] = (char *) NULL;
  *command_size = i;
  free(x);
  return c;
}

void x() {
  printf("Signal  %d!!!\n", getpid());
}

void y() {
  printf("Stopped child---------------------------");
}

void to_foreground() {
  signal(SIGTTOU, SIG_IGN);
  //create a new proces sgroup.
  setpgid(getpid(),getpid());
  //set it as fg group in the terminal
  tcsetpgrp(0,getpgid(getpid()));
}

void to_background() {
//to be called only if to_foreground hasn't been
  signal(SIGTTOU, SIG_IGN);
  //create a new proces sgroup.
  setpgid(getpid(),getpid());
}

int main() {
  
  // signal(SIGINT, SIG_IGN); // ctrl+c
  signal(SIGTSTP, x); //ctrl+z
  signal(SIGQUIT, SIG_IGN); // ctrl+\
  signal(SIGCHLD, y); // ctrl+\

  do {
    if (DEBUG) {
      printf("Entering\n");
      fflush(stdout);
    }
    int bckgrnd, cmd_size;
    char **c = read_command(&bckgrnd, &cmd_size);
    x();
    if (DEBUG) {
      if (bckgrnd)
        printf("Background\n");
      else
        printf("Foreground\n");
    }

    if (c[0]!= NULL && strcmp(c[0],"start")==0 && cmd_size==2) {
      int cpid = atoi(c[1]);
      if (cpid <= 0) {
        perror("Invalid pid");
      }
      kill(cpid, SIGCONT);
    }
    else if (c[0]!= NULL && strcmp(c[0],"stop")==0 && cmd_size==2) {
      int cpid = atoi(c[1]);
      if (cpid <= 0) {
        perror("Invalid pid");
      }
      kill(cpid, SIGTSTP);
    }
    else if (c[0]!= NULL && strcmp(c[0],"stop")==0 && cmd_size==2) {

    }
    else if (c[0]!= NULL) {
      pid_t child_pid;
      if ((child_pid=fork()) < 0) {
        perror("Fork error");
      }
      else if (child_pid == 0) {
        if (DEBUG2) {
          printf("In child\n");
        }
        // signal(SIGTTOU, SIG_IGN);

        // if (DEBUG) 
        //   printf("fg group is %d\n", tcgetpgrp(0)); 

        // //create a new proces sgroup.
        // setpgid(getpid(),getpid());
        // //set it as fg group in the terminal
        // tcsetpgrp(0,getpgid(getpid()));

        // if (DEBUG) {
        //   int qw = 0;
        //   scanf("%d", &qw);
        //   printf("fg group is %d  %d\n", tcgetpgrp(0), qw); 
        //   fflush(stdout);
        // }

        if (!bckgrnd)
          to_foreground();
        else
          to_background();

        if (DEBUG)
          x();
        // signal(SIGTSTP, SIG_DFL);
        int temp = execvp(c[0], c);
        if (temp<0)
          perror("Exec Error");
      }
      else {
        /*
        int *stat;
        do {
          printf("%d", waitpid(-1,stat, WEXITED|WSTOPPED|WUNTRACED));
          // wait(&stat);
          // printf("S\n");
        }
        while (WIFEXITED(stat) || (WIFSTOPPED(stat) && WSTOPSIG(stat) == SIGTSTP));
        */
        if (DEBUG2) {
          printf("In parent\n");
        }
        int stat = 0;
        int pid;
        if (!bckgrnd) {
          pid = waitpid(child_pid, &stat, WUNTRACED);
          to_foreground();
        }
        else {
          // to_foreground();
          pid = waitpid(child_pid, &stat, WNOHANG);
          if (DEBUG) {
            printf("Parent waitpid: %d\n", pid);
          }
          if (TEST_SLEEP) { 
            sleep(3);
            printf("\nSleeping...\n");
            kill(child_pid, SIGTSTP);
            sleep(2);
            printf("\nWaking...\n");
            kill(child_pid, SIGCONT);
          }
          // while (waitpid((pid_t) (-1), 0, WNOHANG) > 0) {printf("Collecting...\n");}
        }

        if (DEBUG) {
          if (bckgrnd)
            printf("%d %d %d\n", pid, stat, WIFEXITED(stat));
          printf("\nBack to parent\n");
        }
      }
    }
    else if (DEBUG2) {
      printf("Null detected\n");
    }
    if (DEBUG) {
      printf("Almost out\n");
    }
  }
  while(1);
  while (waitpid((pid_t) (-1), 0, WNOHANG) > 0) {printf("Collecting...\n");}
  return 0;
}


