#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>

#include <ifaddrs.h>
#include <arpa/inet.h>

#define LISTEN_BCKLG 5

#define DEBUG_UNIX_SOCKET 1
#define DEBUG 1

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

int child;
int in_parent;

int n;
int port;

int *is_child_busy;
pid_t *child_pids;
int **unix_domain_sockets;

void parse_arguments(int argc, char **argv) {
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
}

void initialize_arrays() {
  is_child_busy = (int *) malloc(sizeof(int) * n);
  child_pids = (pid_t *) malloc(sizeof(pid_t) * n);

  unix_domain_sockets = (int **) malloc(sizeof(int *) * n);

  int i;

  for (i = 0; i < n; i++) {
    unix_domain_sockets[i] = (int *) malloc(sizeof(int) * 2);
  }
}

void deinitialize_arrays() {

  free(is_child_busy);
  free(child_pids);

  int i;
  for (i = 0; i < n; i++) {
    free(unix_domain_sockets[i]);
  }
  free(unix_domain_sockets);
}

void setup_unix_ds() {
  int i;

  for (i = 0; i < n; i++) {
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, unix_domain_sockets[i]) == -1) {
      perror("Error creating socketpair");
      exit(1);
    }
  }
}

void generate_children() {
  int i;
  in_parent = 1;

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
}

void close_unix_ds() {
  int i;
  if (in_parent) {
    for (i = 0; i < n; i++)
      close(unix_domain_sockets[i][1]);
  }
  else {
    for (i = 0; i < n; i++) {
      close(unix_domain_sockets[i][0]);
      if (i != child)
        close(unix_domain_sockets[i][1]);
    }
  }
}

void test_unix_socket() {
  if (in_parent) {

    int pd = getpid();
    int npid = htonl(pd);

    int i;
    for (i = 0; i < n; i++)
      write(unix_domain_sockets[i][0], &npid, sizeof(npid));

    for (i = 0; i < n; i++) {
      read(unix_domain_sockets[i][0], &pd, sizeof(pd));
      npid = ntohl(pd);
      printf("Parent %d: Recd %d\n", getpid(), npid);
    }

  }
  else {
    int pd;
    read(unix_domain_sockets[child][1], &pd, sizeof(pd));
    int rpid = ntohl(pd);
    printf("Child %d: Recd %d\n", getpid(), rpid);

    pd = getpid();
    rpid = htonl(pd);
    write(unix_domain_sockets[child][1], &rpid, sizeof(rpid));
  }
}

void int_handler() {
  printf("INT!\n");
  /*
  FILL
  */
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

void handle_echo_server(long acc_conn) {

  if (DEBUG)
    printf("Opening connection in child: %d PID: %d FD: %ld\n", child, getpid(), acc_conn);

  char buff[20000];
  int bytes;
  while ((bytes = recv (acc_conn, buff, sizeof (buff), 0)) != EOF && bytes != 0) {
    buff[bytes] = '\0';
    printf ("%d: %s", bytes, buff);
    send (acc_conn, buff, strlen (buff), 0);
  }

  if (DEBUG)
    printf("Connection Closed with %d bytes\n", bytes);

  close(acc_conn);
}

// The code in the following function has been adapted from childProcess() code in lab sheet 7
void send_fd(int sock, int fd) {
  struct iovec vec;
  vec.iov_base = "";
  vec.iov_len = strlen("") + 1;

  struct cmsghdr * cmessage;
  cmessage = alloca(sizeof(struct cmsghdr) + sizeof(fd));
  cmessage->cmsg_level = SOL_SOCKET;
  cmessage->cmsg_type = SCM_RIGHTS;
  cmessage->cmsg_len = sizeof(struct cmsghdr) + sizeof(fd);

  memcpy(CMSG_DATA(cmessage), &fd, sizeof(fd));

  struct msghdr message;
  message.msg_iov = &vec;
  message.msg_iovlen = 1;
  message.msg_name = NULL;
  message.msg_namelen = 0;
  message.msg_control = cmessage;
  message.msg_controllen = cmessage->cmsg_len;

  if (DEBUG)
    printf("Parent process %d sending fd %d\n", getpid(), fd);

  if (sendmsg(sock, &message, 0) != vec.iov_len) {
    perror("sendmsg() in send_fd()");
    exit(1);
  }
}

// The code in the following function has been adapted from parentProcess() code in lab sheet 7
int receive_fd(int sock) {
  char buf[80];
  struct iovec vec;
  vec.iov_base = buf;
  vec.iov_len = 80;

  int fd;

  struct cmsghdr * cmessage;
  cmessage = alloca(sizeof(struct cmsghdr) + sizeof(fd));
  cmessage->cmsg_len = sizeof(struct cmsghdr) + sizeof(fd);

  struct msghdr message;
  message.msg_iov = &vec;
  message.msg_iovlen = 1;
  message.msg_name = NULL;
  message.msg_namelen = 0;
  message.msg_control = cmessage;
  message.msg_controllen = cmessage->cmsg_len;

  if (!recvmsg(sock, &message, 0))
    return -1;

  memcpy(&fd, CMSG_DATA(cmessage), sizeof(fd));

  if (DEBUG)
    printf("Child process %d receiving fd %d\n", getpid(), fd);

  return fd;
}

void send_termination_one(int fd) {
  int nw_one = htons(1);
  write(fd, &nw_one, sizeof(nw_one));
}

void receive_termination_one(int fd) {
  int nw_one;
  read(fd, &nw_one, sizeof(nw_one));

  if (DEBUG)
    printf("Received: %d\n", ntohs(nw_one));
}

int main(int argc, char **argv) {

  parse_arguments(argc, argv);

  initialize_arrays();

  setup_unix_ds();

  generate_children();

  close_unix_ds();

  if (DEBUG_UNIX_SOCKET)
    test_unix_socket();

  if (in_parent) {
    setup_signal_handler();
    int listening_fd = setup_tcp();
    struct sockaddr_in incoming_client_addr;
    int client_addr_len = sizeof(incoming_client_addr);

    fd_set read_set;

    FD_ZERO(&read_set);

    int max_fd;

    int i;

    for (i = 0; i < n; i++)
      max_fd = MAX(unix_domain_sockets[i][0], max_fd);

    max_fd = MAX(listening_fd, max_fd);

    max_fd++;

    while (1) {

      for (i = 0; i < n; i++)
        FD_SET(unix_domain_sockets[i][0], &read_set);

      FD_SET(listening_fd, &read_set);

      select(max_fd, &read_set, NULL, NULL, NULL);

      if (FD_ISSET(listening_fd, &read_set)) {
        int acc_conn = accept(listening_fd, (struct sockaddr *) &incoming_client_addr, &client_addr_len);

        // printf("Accepted!\n");
        int client_port = ntohs(incoming_client_addr.sin_port);
        char ip_buf[100];
        inet_ntop(AF_INET, &incoming_client_addr.sin_addr, ip_buf, 100);

        printf("IP: %s Port: %d FD: %d\n", ip_buf, client_port, acc_conn);

        long nw_acc_sock = htonl((long)acc_conn);

        for (i = 0; i < n; i++)
          if (!is_child_busy[i]) {
            // write(unix_domain_sockets[i][0], &nw_acc_sock, sizeof(nw_acc_sock));
            send_fd(unix_domain_sockets[i][0], acc_conn);
            close(acc_conn);
            is_child_busy[i] = 1;
            break;
          }
      }

      for (i = 0; i < n; i++)
        if (FD_ISSET(unix_domain_sockets[i][0], &read_set)) {
          is_child_busy[i] = 0;
          receive_termination_one(unix_domain_sockets[i][0]);
          printf("Child %d free\n", i);
        }

    }
  }
  else {
    while (1) {
      long nw_acc_sock;
      // read(unix_domain_sockets[child][1], &nw_acc_sock, sizeof(nw_acc_sock));
      long acc_sock = receive_fd(unix_domain_sockets[child][1]); //ntohl(nw_acc_sock);
      handle_echo_server(acc_sock);
      send_termination_one(unix_domain_sockets[child][1]);
    }
  }

  deinitialize_arrays();

  return 0;
}