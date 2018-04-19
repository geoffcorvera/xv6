#ifdef CS333_P2
#include "types.h"
#include "uproc.h"
#include "user.h"

#define MAXPROC 16 // arbitrary value

int
main(void)
{
  struct uproc *p;
  struct uproc *ptable = malloc(MAXPROC * sizeof(struct uproc));

  printf(1,
      "PID\tUID\tGID\tPPID\tElapsed Time\tCPU Time\tState\tSize\tName\n");

  ptable[0] = (struct uproc){1,1,1,1,1,1,"Runnable",1,"Fake Proc"};
  p = ptable;
  printf(1, "PID: %d\n", p->pid);

  free(ptable);

  exit();
}
#endif
