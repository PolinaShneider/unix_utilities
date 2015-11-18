#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#define LISTEN_BCKLG 5

#define DEBUG 1

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

int child;
int in_parent;

int n;
int port;

struct fd_linkedlist {
  int fd;
  struct fd_linkedlist *next;
};

struct group_linkedlist {
  struct fd_linkedlist *fd;
  char *group_name;
  struct group_linkedlist *next;
};

struct fd_linkedlist *fds;
struct group_linkedlist *groups;

// int *is_child_busy;
// pid_t *child_pids;
int **unix_domain_sockets;

void parse_arguments(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./select_server.out PORT\n");
    exit(0);
  }

  int i;

  i = 0;
  while (argv[1][i] != '\0') {
    if (argv[1][i] < '0' || argv[1][i] > '9') {
      fprintf(stderr, "Error: %s is not a positive integer\nUsage: ./select_server.out PORT\n", argv[1]);
      exit(0);
    }
    i++;
  }

  port = atoi(argv[1]); //printf("%d\n", n);
}

int setup_tcp() {
  int fd = socket(PF_INET, SOCK_STREAM, 0);

  if (fd == -1)
    perror("Error in socket()");

  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
    perror("Error in bind()");

  if (listen(fd, LISTEN_BCKLG) == -1)
    perror("Error in listen()");

  int flags;
  if ((flags = fcntl(fd, F_GETFL, 0)) >= 0) {
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
      perror("Error setting fd flag to non-blocking");
  }
  else {
    perror("Error getting fd flag");
  }

  return fd;
}

void initialize() {
  fds = NULL;
  groups = NULL;


}

void deinitialize_structs() {

  // TODO:

}

void handle_echo_server(long acc_conn) {

  if (DEBUG)
    printf("Opening connection in child: %d PID: %d FD: %ld\n", child, getpid(), acc_conn);

  char buff[20000];
  int bytes;
  while ((bytes = recv (acc_conn, buff, sizeof (buff), 0)) != EOF && bytes != 0) {
    buff[bytes] = '\0';

    if (DEBUG)
      printf ("%d: %s", bytes, buff);

    send (acc_conn, buff, strlen (buff), 0);
  }

  if (DEBUG)
    printf("Connection Closed with %d bytes\n", bytes);

  if (close(acc_conn) == -1)
    perror("Error in closing fd after echo");
}

struct fd_linkedlist *new_fd_linkedlist_member(int acc_conn) {
  struct fd_linkedlist *new_mem = (struct fd_linkedlist *) malloc(sizeof(struct fd_linkedlist));
  new_mem->fd = acc_conn;
  new_mem->next = NULL;
}

void delete_fd_linkedlist_member(struct fd_linkedlist *mem) {
  struct fd_linkedlist *i_fd = fds;

  if (fds == mem) {
    fds = fds->next;
    free(i_fd);
    return;
  }

  struct fd_linkedlist *prev_fd = i_fd;
  i_fd = i_fd->next;

  while (i_fd != NULL) {
    if (i_fd == mem) {
      prev_fd->next = i_fd->next;
      free(i_fd);
      break;
    }
    else {
      prev_fd = i_fd;
      i_fd = i_fd->next;
    }
  }
}

void receive_termination_one(int fd) {
  int nw_one;
  read(fd, &nw_one, sizeof(nw_one));

  if (DEBUG)
    printf("Received: %d\n", ntohs(nw_one));

  if (ntohs(nw_one) != 1)
    printf("Warning: Received termination %d instead of termination 1\n", ntohs(nw_one));
}

void close_parent_fds(int listening_fd) {
  int i;

  for (i = 0; i < n; i++)
    if (close(unix_domain_sockets[i][0]) == -1)
      perror("Error closing UDS in parent");

  if (close(listening_fd) == -1)
    perror("Error closing TCP socket");
}

void close_child_fd() {
  if (close(unix_domain_sockets[child][1]) == -1)
    perror("Error closing UDS in child");
}

int main(int argc, char **argv) {

  parse_arguments(argc, argv);

  initialize();

  int listening_fd = setup_tcp();

  struct sockaddr_in incoming_client_addr;
  int client_addr_len = sizeof(incoming_client_addr);

  fd_set read_set;
  int max_fd;

  int i;
  struct fd_linkedlist *i_fd;

  while (1) {

    FD_ZERO(&read_set);

    max_fd = listening_fd;

    i_fd = fds;

    while (i_fd != NULL) {
      FD_SET(i_fd->fd, &read_set);
      max_fd = MAX(max_fd, i_fd->fd);
      i_fd = i_fd->next;
    }

    max_fd++;

    FD_SET(listening_fd, &read_set);

    int sel_ret;

    if ((sel_ret = select(max_fd, &read_set, NULL, NULL, NULL)) == -1) {
      if (errno != EINTR)
        perror("select()");
      continue;
    }

    if (FD_ISSET(listening_fd, &read_set)) {

      int acc_conn = accept(listening_fd, (struct sockaddr *) &incoming_client_addr, &client_addr_len);

      if (DEBUG)
        printf("Client accepted at %d\n", acc_conn);

      if (acc_conn == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
          perror("Error in accept()");
        else if (DEBUG)
          perror("Error in accept()");
      }

      i_fd = fds;

      if (i_fd == NULL) {
        fds = new_fd_linkedlist_member(acc_conn);

        if (DEBUG)
          printf("Adding new member to fd_linkedlist\n");
      }
      else {
        while (i_fd -> next != NULL) {
          i_fd = i_fd->next;
        }

        i_fd->next = new_fd_linkedlist_member(acc_conn);

        if (DEBUG)
          printf("Adding to end of fd_linkedlist\n");
      }

    }


    i_fd = fds;

    while (i_fd != NULL) {
      if (FD_ISSET(i_fd->fd, &read_set)) {

        char buff[10];
        int bytes;
        int rcv_size = recv (i_fd->fd, buff, sizeof (buff), 0);

        if (rcv_size == 0) {
          close(i_fd->fd);
          delete_fd_linkedlist_member(i_fd);
        }

        buff[rcv_size-1] = '\0';

        if (DEBUG)
          printf("Received %d bytes from client %d: %s\n", rcv_size, i_fd->fd, buff);

      }
      i_fd = i_fd->next;
    }

  }

  // this should never be called
  close_parent_fds(listening_fd);

  // this should never be called
  // deinitialize_structs();

  return 0;
}