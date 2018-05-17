#ifdef CS333_P2
#include "types.h"
#include "uproc.h"
#include "user.h"

#define MAXPROC 16 // arbitrary value

#ifdef CS333_P3P4
#define HEADER "PID\tName\t\tUID\tGID\tPPID\tPrio\tElapsed\tCPU Time   State\tSize\n"
#define ROW "%d\t%d\t%d\t%d\t%d\t%d\t   %s\t%d\n"
#else
#define HEADER "PID\tName\t\tUID\tGID\tPPID\tElapsed\tCPU Time   State\tSize\n"
#define ROW "%d\t%d\t%d\t%d\t%d\t   %s\t%d\n"
#endif



int
main(void)
{
  //struct uproc *p;
  struct uproc *ptable = malloc(MAXPROC * sizeof(struct uproc)),
               p;
  int numProcs;

  numProcs = getprocs(MAXPROC, ptable);

  if(numProcs < 0) {
    printf(2, "Error: getprocs call failed. %s at line %d\n",
        __FILE__, __LINE__);
    free(ptable);
    exit();
  }

  printf(1, HEADER);
  for(int i = 0; i < numProcs; i++) {
    p = ptable[i]; 
    printf(1, "%d\t%s%s", p.pid, p.name, strlen(p.name) > 4 ? "\t" : "\t\t");
    printf(1, ROW,
        p.uid, p.gid, p.ppid,
#ifdef CS333_P3P4
        p.prio,
#endif
        p.elapsed_ticks,
        p.CPU_total_ticks,
        p.state, p.size
    );
  }

  free(ptable);
  exit();
}
#endif
