#ifdef CS333_P3P4
#include "types.h"
#include "user.h"

int
main() {
  int prio, pid;

  pid = 0;

  for(prio=0; prio < 10; prio++)
    setpriority(pid, prio);

  exit();
}
#endif
