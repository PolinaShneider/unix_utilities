#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>

#define LISTEN_BCKLG 5

#define DEBUG_UNIX_SOCKET 1
#define DEBUG 1

int child;
int in_parent;
int master;
int n;
int port;

int bcast_count;

int *is_child_busy;
pid_t *child_pids;

void int_handler() {
  printf("INT!\n");
}

void setup_signal_handler() {
  struct sigaction sa;
  sa.sa_handler = &int_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGINT, &sa, 0) == -1) {
    perror("Unable to set SIGINT action");
  }
}

int setup_tcp() {
  int fd = socket(PF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  bind(fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

  listen(fd, LISTEN_BCKLG);

  return fd;
}

int setup_unix_ds_server() {
  if (DEBUG)
    printf("Setting up unix socket of child %d\n", child);

  int fd;
  if ((fd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
    perror("Error in socket() call");
  }

  struct sockaddr_un unix_server_address;
  bzero(&unix_server_address, sizeof(unix_server_address));
  unix_server_address.sun_family = AF_LOCAL;

  char buf[20];

  sprintf(buf, "u%dnix_socket", child);

  strncpy(unix_server_address.sun_path, buf, sizeof(unix_server_address.sun_path) - 1);

  unlink(buf);
  if (bind(fd, (struct sockaddr *) &unix_server_address, sizeof(unix_server_address)) == -1) {
    perror("Error in bind() call");
  }

  if (DEBUG)
    printf("bind() called in child %d with name %s\n", child, unix_server_address.sun_path);

  if (listen(fd, 5) == -1) {
    perror("Error in listen() call");
  }

  if (DEBUG)
    printf("listen() called in child %d on fd %d\n", child, fd);

  int acc_fd;

  if ((acc_fd = accept(fd, NULL, NULL)) == -1) {
    perror("Error in accept() call");
  }

  if (DEBUG)
    printf("accept() called in child %d on fd %d\n", child, acc_fd);

  return acc_fd;
}

int *setup_unix_connections() {

  struct sockaddr_un unix_server_address;
  bzero(&unix_server_address, sizeof(unix_server_address));
  unix_server_address.sun_family = AF_LOCAL;

  char buf[20];

  int i;

  int *x = (int*) malloc(sizeof(int) * n);

  for (i = 0; i < n; i++) {
    x[i] = socket(AF_LOCAL, SOCK_STREAM, 0);

    if (x[i] == -1)
      perror("socket() called in parent");

    sprintf(buf, "u%dnix_socket", i);
    strncpy(unix_server_address.sun_path, buf, sizeof(unix_server_address.sun_path) - 1);

    unlink(buf);

    if (connect(x[i], (struct sockaddr *) &unix_server_address, sizeof(unix_server_address)) == -1)
      perror("connect() called in parent");
  }

  return x;
}

int main(int argc, char **argv) {

  if (argc != 2 && argc != 3) {
    fprintf(stderr, "Usage: ./uds.out PORT [N]\n");
    exit(0);
  }

  int i;

  i = 0;
  while (argv[1][i] != '\0') {
    if (argv[1][i] < '0' || argv[1][i] > '9') {
      fprintf(stderr, "Error: %s is not a positive integer\nUsage: ./uds.out PORT [N]\n", argv[1]);
      exit(0);
    }
    i++;
  }

  port = atoi(argv[1]); //printf("%d\n", n);

  i = 0;

  if (argc == 3) {
    while (argv[2][i] != '\0') {
      if (argv[2][i] < '0' || argv[2][i] > '9') {
        fprintf(stderr, "Error: %s is not a positive integer\nUsage: ./uds.out PORT [N]\n", argv[2]);
        exit(0);
      }
      i++;
    }

    n = atoi(argv[2]); //printf("%d\n", n);
  }
  else {
    printf("Number of child processes not specified. Using 5 by default.\n");
    n = 5;
  }

  is_child_busy = (int *) malloc(sizeof(int) * n);
  child_pids = (pid_t *) malloc(sizeof(pid_t) * n);

  int in_parent = 1;

  for (i = 0; i < n; i++) {
    pid_t temp;
    if ((temp = fork()) == 0) {
      in_parent = 0;
      child = i;
      break;
    }
    else {
      child_pids[i] = temp;
    }

    is_child_busy[i] = 0;
  }

  if (in_parent) {
    setup_signal_handler();

    int listening_fd = setup_tcp();

    sleep(3);

    int *child_unix = setup_unix_connections();

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    int connected_fd;

    if (DEBUG_UNIX_SOCKET) {
      printf("Parent PID: %d\n", getpid());
      int i, pid_num;
      pid_num = getpid();
      for (i = 0; i < n; i++)
        write(child_unix[i], &pid_num, 1);

      printf("DONE\n");
    }
    while (1) {
      //   connected_fd = accept(listening_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    }

  }
  else {

    int unix_fd = setup_unix_ds_server();

    if (DEBUG)
      printf("In child %d\n", child);

    int num;

    if (DEBUG_UNIX_SOCKET) {
      read(unix_fd, &num, sizeof(&num));
      printf("%d: recd. %d\n", getpid(), num);
    }
  }

  free(is_child_busy);
  free(child_pids);

  return 0;
}
