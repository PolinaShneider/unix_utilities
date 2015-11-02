#include <stdio.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <errno.h>
#include <string.h>


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

void system_call_line(int argc, char *argv[]) {

  print_debug("Here- Line!!\n");

  pid_t child_pid;

  if ((child_pid = fork()) == 0) {
    print_debug("Child: Entering\n");

    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
      perror("ptrace in Child");
    }

    print_debug("Child: After ptrace\n");

    // if (raise(SIGSTOP) != 0) {
    //   perror("raise stop signal in child");
    // }

    // print_debug("Child: After STOP\n");

    if (execvp(argv[2], argv + 2) == -1) {
      perror("Exec error");
    }
  }
  else {
    int stat;

    print_debug("Parent: Entering...\n");

    while (1) {

      int stat;
      wait(&stat);

      if (WIFEXITED(stat)) {
        print_debug("Done...\n");
        break;
      }

      struct user_regs_struct regists;

      ptrace(PTRACE_GETREGS, child_pid, NULL, &regists);

      unsigned long long ip;

      #if _____LP64_____
        ip = regists.rip;
      #elif __x86_64__
        ip = regists.rip;
      #else
        ip = regists.eip;
      #endif

      int instr = ptrace(PTRACE_PEEKTEXT, child_pid, ip, NULL);

      printf("IP: %llx Instruction: %x\n", ip, instr);

      ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
    }

  }

}

void system_call_print(int argc, char *argv[]) {

  print_debug("Here- Syscall!!\n");

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

      if (!WIFSTOPPED(stat) || !(/*(WSTOPSIG(stat) == SIGTRAP) && */(WSTOPSIG(stat) & 0x80))) {
        continue;
      }

      int syscall_num, return_val;

      syscall_num = ptrace(PTRACE_PEEKUSER, child_pid, sizeof(long) * ORIG_EAX);

      ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);

      print_debug("Signal...\n");

      // tracee enters ptrace stop
      if (waitpid(child_pid, &stat, WUNTRACED) == -1)
        perror("Waiting for signal");

      if (WIFEXITED(stat)) {
        print_debug("Done...\n");
        break;
      }

      if (!WIFSTOPPED(stat) || !(WSTOPSIG(stat) & 0x80)) {
        continue;
      }

      return_val = ptrace(PTRACE_PEEKUSER, child_pid, sizeof(long) * EAX);

      if (return_val >= 0) {
        fprintf(stderr, "System call: %d Return: %d\n", syscall_num, return_val);
      }
      else {
        fprintf(stderr, "System call: %d Return: -1 (%s)\n", syscall_num, strerror(return_val * -1));
      }

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

}

int main(int argc, char *argv[]) {

  if (argc == 1 || (argc == 2 && strcmp(argv[1], "-line") == 0)) {
    fprintf(stderr, "Error: Syntax is\n./like_strace.out [-line] prog [args]\n");
  }

  if (strcmp(argv[1], "-line") == 0) {
    system_call_line(argc, argv);
  }
  else {
    system_call_print(argc, argv);
  }

  return 0;
}

// References:
// http://theantway.com/2013/01/notes-for-playing-with-ptrace-on-64-bits-ubuntu-12-10/
// http://www.linuxjournal.com/node/6100/print
// https://mikecvet.wordpress.com/2010/08/14/ptrace-tutorial/
// https://github.com/nelhage/ministrace
// https://blog.nelhage.com/2010/08/write-yourself-an-strace-in-70-lines-of-code/
