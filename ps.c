#ifdef CS333_P2
#include "types.h"
#include "uproc.h"
#include "user.h"

#define MAXPROC 16 // arbitrary value

int
main(void)
{
  //struct uproc *p;
  struct uproc *ptable = malloc(MAXPROC * sizeof(struct uproc)),
               p;
  int numProcs;

  numProcs = getprocs(MAXPROC, ptable);
  // TODO: should I use panic() here?
  if(numProcs < 0)
    return -1;

  printf(1,
      "PID\tUID\tGID\tPPID\tElapsed\tCPU Time   State\tSize\tName\n");
  for(int i = 0; i < numProcs; i++) {
    p = ptable[i]; 
    printf(1, "%d\t%d\t%d\t%d\t%d\t%d\t   %s\t%d\t%s\n",
        p.pid, p.uid,
        p.gid, p.ppid,
        p.elapsed_ticks,
        p.CPU_total_ticks,
        p.state, p.size,
        p.name
    );
  }

  free(ptable);
  exit();
}
#endif
