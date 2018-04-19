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
  uint max, numProcs;
  struct uproc *ptable,
               *p;

  if(argint(0, (int*)&max) < 0)
    return -1;
  if(argptr(1, (void*)&ptable, sizeof(struct uproc)) < 0)
    return -1;

  for(p = ptable; p < &ptable[max]; p++) {
    p->pid = 1;
    p->uid = 1;
    p->gid = 1;
    p->ppid = 1;
    p->elapsed_ticks = 1;
    p->CPU_total_ticks = 1;
    //p->state = s;
    p->size = 1;
    //p->name = n;
  }

  numProcs = 5;
  return numProcs;
}
#endif
