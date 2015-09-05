#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define DEBUG 1

char **read_command(int *is_background) {

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
      break;
    }
    else if (t[strlen(t)-1] == '&') {
      *is_background = 1;
      t[strlen(t)-1] = '\0';
    }

    c[++i] = (char *)malloc((strlen(t))*sizeof(char));
    strcpy(c[i],t);
  }
  c[++i] = (char *) NULL;

  free(x);
  return c;
}

void x() {
  printf("Signal  %d!!!\n", getpid());
}

int main() {
  
  // signal(SIGINT, SIG_IGN); // ctrl+c
  signal(SIGTSTP, x); //ctrl+z
  signal(SIGQUIT, SIG_IGN); // ctrl+\

  int bckgrnd;
  char **c = read_command(&bckgrnd);
  x();
  if (DEBUG) {
    if (bckgrnd)
      printf("Background\n");
    else
      printf("Foreground\n");
  }

  if (c[0]!= NULL) {
    int child_pid;
    if ((child_pid=fork()) == 0) {

      signal(SIGTTOU, SIG_IGN);

      if (DEBUG) 
        printf("fg group is %d\n", tcgetpgrp(0)); 

      //create a new proces sgroup.
      setpgid(getpid(),getpid());
      //set it as fg group in the terminal
      tcsetpgrp(0,getpgid(getpid()));

      if (DEBUG) {
        int qw = 0;
        scanf("%d", &qw);
        printf("fg group is %d  %d\n", tcgetpgrp(0), qw); 
        fflush(stdout);
      }

      x();
      // signal(SIGTSTP, SIG_DFL);
      if (execvp(c[0], c)<0)
        perror("Exec Error");
    }
    else {
      int *stat;
      do {
        printf("%d", waitpid(-1,stat, WEXITED|WSTOPPED|WUNTRACED));
        // wait(&stat);
        // printf("S\n");
      }
      while (WIFEXITED(stat) || (WIFSTOPPED(stat) && WSTOPSIG(stat) == SIGTSTP));
      if (DEBUG)
        printf("Back to parent\n");
      // exit(0);
    }
  }
}


