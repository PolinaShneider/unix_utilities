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

struct fd_linkedlist *new_fd_linkedlist_member(int acc_conn) {
  struct fd_linkedlist *new_mem = (struct fd_linkedlist *) malloc(sizeof(struct fd_linkedlist));
  new_mem->fd = acc_conn;
  new_mem->next = NULL;
  return new_mem;
}

struct group_linkedlist *new_grp_linkedlist_member(struct fd_linkedlist *fd_mem, char *group_name) {
  struct group_linkedlist *new_mem = (struct group_linkedlist *) malloc(sizeof(struct group_linkedlist));
  new_mem->fd = fd_mem;

  new_mem->group_name = (char *) malloc(sizeof(char) * (strlen(group_name) + 1));
  strcpy(new_mem->group_name, group_name);

  new_mem->group_name[strlen(group_name)] = '\0';

  if (DEBUG) {
    printf("Length: %d (old) vs %d (new)\n", (int)strlen(group_name), (int)strlen(new_mem->group_name));
    printf("Old group name: %s\n", group_name);
    printf("New group name: %s\n", new_mem->group_name);
  }

  new_mem->next = NULL;
  return new_mem;
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

void close_parent_fds(int listening_fd) {

  if (close(listening_fd) == -1)
    perror("Error closing TCP socket");
}

void add_to_group(char *group_name, struct fd_linkedlist *add_fd) {

  struct group_linkedlist *i_grp = groups;

  if (DEBUG)
    printf("Add group name: %s\n", group_name);

  while (i_grp != NULL && i_grp->next != NULL) {
    if (!strcmp(group_name, i_grp->group_name)) {

      struct fd_linkedlist *i_fd = i_grp->fd;

      if (i_fd->fd == add_fd->fd)
        return;

      while (i_fd -> next != NULL) {
        i_fd = i_fd->next;

        if (i_fd->fd == add_fd->fd)
          return;
      }

      i_fd->next = add_fd;
      return;
    }
    else {
      i_grp = i_grp->next;
    }
  }

  if (i_grp == NULL) {

    if (DEBUG)
      printf("Adding new group %s to NULL list\n", group_name);

    groups = new_grp_linkedlist_member(add_fd, group_name);
  }
  else {

    if (DEBUG)
      printf("Adding new group %s to non-NULL list\n", group_name);

    i_grp->next = new_grp_linkedlist_member(add_fd, group_name);
  }

}

void send_to_fd_list(char *buff, struct fd_linkedlist *i_fd) {

  if (DEBUG)
    printf("Sending to list: %s", buff);

  while (i_fd != NULL) {
    send (i_fd->fd, buff, strlen (buff), 0);
    i_fd = i_fd->next;
  }
}

void send_to_all(char *buff) {
  send_to_fd_list(buff, fds);
}

void send_msg_to_group(char *grp_msg_buff) {
  struct group_linkedlist *i_grp = groups;
  int i;
  for (i = 0; grp_msg_buff[i] != '$' && i < strlen(grp_msg_buff); i++);

  if (DEBUG) {
    printf("Groupname: ");
    int j;

    for (j = 0; j < i; j++)
      printf("%c", grp_msg_buff[j]);

    printf(" Length: %d Message: %s", i, grp_msg_buff + i + 1);

  }

  while (i_grp != NULL) {
    if (DEBUG) {
      printf("Comparing first %d of %s and %s\n", i, grp_msg_buff, i_grp->group_name);
    }
    if (!strncmp(grp_msg_buff, i_grp->group_name, i) && (strlen(i_grp->group_name) == i)) {
      if (DEBUG)
        printf("Here\n");

      send_to_fd_list(grp_msg_buff + i + 1, i_grp->fd);
      break;
    }
    else {

      if (DEBUG)
        printf("Values: %d, %s, %d\n", strncmp(grp_msg_buff, i_grp->group_name, i), i_grp->group_name, i);
      i_grp = i_grp->next;
    }
  }
}

void handle_message(char *buff, struct fd_linkedlist *i_fd) {
  if (!strncmp(buff, "GROUP$", 6)) {

    int i;
    for (i = 0; i < strlen(buff); i++) {
      if (buff[i] == '\n' || buff[i] == '\r') {
        buff[i] = '\0';
        break;
      }
    }

    if (DEBUG) {
      printf("A1dding %d to group %s yep\n", i_fd->fd, buff + 6);
    }
    add_to_group(buff + 6, i_fd);
  }
  else if (!strncmp(buff, "GROUPMSG$", 9)) {
    send_msg_to_group(buff + 9);
  }
  else {
    send_to_all(buff);
  }

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

        char buff[20000];
        int bytes;

        int rcv_size;

        int first_run_done = 0;

        while ((rcv_size = recv (i_fd->fd, buff, sizeof (buff) - 1, MSG_DONTWAIT)) != -1) {

          if (rcv_size == 0) {
            close(i_fd->fd);

            if (DEBUG)
              printf("Closing %d\n", i_fd->fd);

            delete_fd_linkedlist_member(i_fd);
            break;
          }

          if (first_run_done)
            continue;
          else
            first_run_done = 1;

          // if (buff[rcv_size - 1] == '\n')
          //   buff[rcv_size - 1] = '\0';
          // else

          buff[rcv_size] = '\0';

          if (DEBUG)
            printf("Received %d bytes from client %d: %s yep\n", rcv_size, i_fd->fd, buff);

          handle_message(buff, i_fd);

        }

        if (rcv_size != 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
          perror("Error when receiving data from client");
        }
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