#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "uproc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start. 
int
sys_uptime(void)
{
  uint xticks;
  
  xticks = ticks;
  return xticks;
}

//Turn of the computer
int
sys_halt(void){
  cprintf("Shutting down ...\n");
  outw( 0x604, 0x0 | 0x2000);
  return 0;
}

#ifdef CS333_P1
int
sys_date(void){
  struct rtcdate *d;
  
  if(argptr(0, (void*)&d, sizeof(struct rtcdate)) < 0)
    return -1;
  cmostime(d);
  
  return 0;
}
#endif

#ifdef CS333_P2
int
sys_getuid(void){
  return proc->uid;
}

int
sys_getgid(void){
  return proc->gid;
}

int
sys_getppid(void){
  return proc->parent->pid;
}

int
sys_setuid(void){
  uint arg;
  if(argint(0, (int*)&arg) < 0 || arg < 0 || arg > 32767)
    return -1;

  proc->uid = arg;
  return 0;
}

int
sys_setgid(void){
  uint arg;
  if(argint(0, (int*)&arg) < 0 || arg < 0 || arg > 32767)
    return -1;

  proc->gid = arg;
  return 0;
}

int
sys_getprocs(void){
  uint max, n;
  struct uproc *aProcs; // active processes

  // check max not greater than number of procs
  if(argint(0, (int*)&max) < 0 || max > NPROC)
    return -1;
  if(argptr(1, (void*)&aProcs, sizeof(struct uproc)) < 0)
    return -1;
  
  n = ptablecopy(aProcs, max);
  return n;
}
#endif

#ifdef CS333_P3P4
//TODO sys_setpriority()
int
sys_setpriority(void){
  int priority, pid; 

  if(argint(0, (int*)&pid) < 0)
    return -1;
  if(argint(1, (int*)&priority) < 0 || priority < 0 || priority > MAXPRIO)
    return -1;

  return 0;
}
#endif
