#include <stdio.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reg.h>
#include <sys/user.h>

//http://stackoverflow.com/questions/7514837/why-does-this-ptrace-program-say-syscall-returned-38

#define DEBUG 0

// http://stackoverflow.com/a/22279093/2427542
// http://stackoverflow.com/a/5272888/2427542
#if _____LP64_____
#define ORIG_EAX ORIG_RAX
#define EAX RAX
#elif __x86_64__
#define ORIG_EAX ORIG_RAX
#define EAX RAX
#endif

void print_debug(char *str) {
  if (DEBUG) {
    printf("%s", str);
  }
}

int main(int argc, char *argv[]) {

  if (argc == 1) {
    fprintf(stderr, "Error: Syntax is\n./like_strace.out [-line] prog [args]\n");
  }

  pid_t child_pid;

  if ((child_pid = fork()) == 0) {
    print_debug("Child: Entering\n");

    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
      perror("ptrace in Child");
    }

    print_debug("Child: After ptrace\n");

    if (raise(SIGSTOP) != 0) {
      perror("raise stop signal in child");
    }

    print_debug("Child: After STOP\n");

    if (execvp(argv[1], argv + 1) == -1) {
      perror("Exec error");
    }
  }
  else {
    int stat;

    print_debug("Parent: Entering...\n");

    if (waitpid(child_pid, &stat, 0) == -1) {
      perror("Waiting for SIGSTOP");
    }
    else if (WIFSTOPPED(stat) == 0) {
      fprintf(stderr, "Error: Strange signal in child\n");
    }

    print_debug("Parent: After waitpid...\n");

    // set options PTRACE_O_TRACESYSGOOD to distinguish ptrace trap from normal
    // trap
    ptrace(PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_TRACESYSGOOD);

    // // to start, and stop at next system call
    // ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);

    // print_debug("Parent: After ptrace\n");

    while (1) {
      print_debug("Parent: In loop\n");

      ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);

      print_debug("Signal...\n");

      // tracee enters ptrace stop
      if (waitpid(child_pid, &stat, WUNTRACED) == -1)
        perror("Waiting for signal");

      if (WIFEXITED(stat)) {
        print_debug("Done...\n");
        break;
      }

      int syscall, retval;

      syscall = ptrace(PTRACE_PEEKUSER, child_pid, sizeof(long) * ORIG_EAX);
      retval = ptrace(PTRACE_PEEKUSER, child_pid, sizeof(long) * EAX);
      fprintf(stderr, "syscall(%d) = %d\n", syscall, retval);

      // // to restart, and stop when child returns
      // ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);

      // // tracee enters ptrace stop
      // if (waitpid(child_pid, &stat, WUNTRACED) == -1)
      //   perror("Waiting for signal");

      // if (WIFEXITED(stat)) {
      //   print_debug("Done...\n");
      //   break;
      // }

      // retval = ptrace(PTRACE_PEEKUSER, child_pid, sizeof(long) * EAX);
      // fprintf(stderr, "%d\n", retval);

      // this is reached when child returns

      // ptrace (PTRACE_CONT, child_pid, NULL, NULL);
    }

  }

  return 0;
}
