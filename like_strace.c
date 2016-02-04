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

#include "linux/defs.h"

struct_sysent syscall_entries[] = {
  #if __x86_64__
  #include "linux/x86_64/syscallent.h"
  #else
  #include "linux/x32/syscallent.h"
  #endif
};

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

int is_error(int err) {
  if (err >= 0)
    return 0;

  char *c = strerror(err * -1);
  char *not = "Unknown error";
  int i;

  for (i=0; i<strlen(not); i++) {
    if (not[i] != c[i])
      break;
  }

  if (i == strlen(not))
    return 0;
  else return 1;
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

      if (!WIFSTOPPED(stat) || !((WSTOPSIG(stat) & 0x80))) {
        continue;
      }

      int syscall_num, return_val;

      syscall_num = ptrace(PTRACE_PEEKUSER, child_pid, sizeof(long) * ORIG_EAX);

      struct user_regs_struct regists;
      ptrace(PTRACE_GETREGS, child_pid, NULL, &regists);

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

      printf("%s (", syscall_entries[syscall_num].sys_name);
      
      int i;

      for (i=0; i < syscall_entries[syscall_num].nargs; i++) {
        switch (i) {
          case 0: printf("%llu", regists.rdi);
          break;
          case 1: printf("%llu", regists.rsi);
          break;
          case 2: printf("%llu", regists.rdx);
          break;
          case 3: printf("%llu", regists.r10);
          break;
          case 4: printf("%llu", regists.r8);
          break;
          case 5: printf("%llu", regists.r9);
          break;
        }

        if (i != syscall_entries[syscall_num].nargs - 1)
          printf(", ");
        else
          printf(")");
      }

      if (return_val >= 0 || !is_error(return_val)) {
        printf("  = %d\n", return_val);
      }
      else {
        printf("  = -1 (%s)\n", strerror(return_val * -1));
      }


    }

  }

}

int main(int argc, char *argv[]) {

  if (argc == 1 || (argc == 2 && strcmp(argv[1], "-line") == 0)) {
    fprintf(stderr, "Error: Syntax is\n./like_strace.out [-line] prog [args]\n");
    return 0;
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
// http://www.linuxjournal.com/node/6100
// https://mikecvet.wordpress.com/2010/08/14/ptrace-tutorial/
// https://github.com/nelhage/ministrace
// https://blog.nelhage.com/2010/08/write-yourself-an-strace-in-70-lines-of-code/
// https://www.win.tue.nl/~aeb/linux/lk/lk-4.html
// http://stackoverflow.com/questions/7514837/why-does-this-ptrace-program-say-syscall-returned-38