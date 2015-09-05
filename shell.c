#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

char **read_command() {

  char *x = NULL;
  char *t;

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
    c[++i] = (char *)malloc((strlen(t))*sizeof(char));
    strcpy(c[i],t);
  }
  c[++i] = (char *) NULL;

  free(x);
  return c;
}

int main() {
  
  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  char **c = read_command();

  if (c[0]!= NULL) {
    if (fork() == 0) {
      if (execvp(c[0], c)<0)
        perror("Exec Error");
    }
    else {
      wait();
    }
  }
}


