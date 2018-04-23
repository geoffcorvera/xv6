#ifdef CS333_P2
#include "types.h"
#include "user.h"

static void leftpad(char *s, int n);

int
main(int argc, char *argv[])
{
  int i, pid, t_start, t_elapsed;
  char *args[argc-1]; // args for child process
  char leadingzeros[3];

  for(i = 1; i < argc; i++)
    args[i-1] = argv[i]; 

  t_start = uptime();
  pid = fork();
  if(pid < 0) {
    printf(1, "fork failed\n");
    exit();
  }
  if(pid == 0) {   // child
    if(exec(args[0], args) < 0) {
      printf(1, "exec failed\n");
      exit();
    }
  }
  else {
    wait();
    t_elapsed = uptime() - t_start;
    leftpad(leadingzeros, t_elapsed % 1000);
    printf(1, "%s took %d.%s%d second\n", argv[1],
        t_elapsed / 1000, leadingzeros,
        t_elapsed % 1000);
  }

  exit();
}

static void
leftpad(char *s, int n)
{
  if(n > 100)
    strcpy("", s);
  else if(n > 10)
    strcpy("0", s);
  else
    strcpy("00", s);
}
#endif
