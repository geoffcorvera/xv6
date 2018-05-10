/* * * * * * * * * * * * * * * * * * * 
 *
 * Bits of procdump() were taken from Mark
 * Morrissey's email.
 *
 * * * * * * * * * * * * * * * * * * */ 

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "uproc.h"

//TODO: refactor ready list manipulations
#ifdef CS333_P3P4
struct StateLists {
  struct proc* ready[MAXPRIO+1];
  struct proc* readyTail[MAXPRIO+1];
  struct proc* free;
  struct proc* freeTail;
  struct proc* sleep;
  struct proc* sleepTail;
  struct proc* zombie;
  struct proc* zombieTail;
  struct proc* running;
  struct proc* runningTail;
  struct proc* embryo;
  struct proc* embryoTail;
};
#endif

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3P4
  struct StateLists pLists;
#endif
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

#if defined(CS333_P3P4)
static void initProcessLists(void);
static void initFreeList(void);
static int stateListAdd(struct proc** head, struct proc** tail, struct proc* p);
static int stateListRemove(struct proc** head, struct proc** tail, struct proc* p);
static int findChildren(struct proc *parent);
static int numberChildren(struct proc *head, struct proc *parent);
static int killFromList(struct proc *head, int pid);
static void assertState(struct proc *p, enum procstate state);
static void stateTransfer(struct proc **fromHead, struct proc **fromTail, enum procstate oldState,
  struct proc **toHead, struct proc **toTail, enum procstate newState, struct proc *p);

static void procdumpP2(struct proc *p, char *state);
#elif defined(CS333_P2)
static void procdumpP2(struct proc *p, char *state);
int ptablecopy(struct uproc* uprocs, int max);
#elif defined(CS333_P1)
static void procdumpP1(struct proc *p, char *state);
#endif

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

#ifndef CS333_P3P4
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
#else
  acquire(&ptable.lock);
  p = ptable.pLists.free;
  if(p == 0){       // no free processes available
    release(&ptable.lock);
    return 0;
  }
  stateTransfer(
      &ptable.pLists.free, &ptable.pLists.freeTail, UNUSED, 
      &ptable.pLists.embryo, &ptable.pLists.embryoTail, EMBRYO, p);
#endif

  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

#if defined(CS333_P1)
  p->start_ticks = ticks;
#elif defined(CS333_P2)
  // initialize to zero
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

#ifdef CS333_P3P4
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
#endif

  p = allocproc();    // moves a process from free list to embryo list
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

#ifdef CS333_P2
  p->uid = UIDINIT;  // default uid
  p->gid = GIDINIT;  // default gid
  p->parent = 0;     // first proc doesn't have parent
#endif

#ifdef CS333_P3P4
  p->next = 0;       // set next to null for first process
  acquire(&ptable.lock);
  stateTransfer(&ptable.pLists.embryo, &ptable.pLists.embryoTail, EMBRYO,
    &ptable.pLists.ready, &ptable.pLists.readyTail, RUNNABLE, p);
  release(&ptable.lock);
#else
  p->state = RUNNABLE;
#endif
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

#ifdef CS333_P2
  np->uid = proc->uid;
  np->gid = proc->gid;
#endif

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
#ifdef CS333_P3P4
  stateTransfer(&ptable.pLists.embryo, &ptable.pLists.embryoTail, EMBRYO
      ,&ptable.pLists.ready, &ptable.pLists.readyTail, RUNNABLE, np);
#else
  np->state = RUNNABLE;
#endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifndef CS333_P3P4
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
#ifdef CS333_P3P4
  stateTransfer(&ptable.pLists.running, &ptable.pLists.runningTail, RUNNING
      ,&ptable.pLists.zombie, &ptable.pLists.zombieTail, ZOMBIE, proc);
#else
  proc->state = ZOMBIE;
#endif
  sched();
  panic("zombie exit");

}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifndef CS333_P3P4
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    //get number of children
    havekids = findChildren(proc);
    p = ptable.pLists.zombie;
    //look for zombie children; skip if empty
    while(p) {
      if(p->parent != proc) {
        p = p->next;
        continue;
      }
      havekids++;
      if(p->state != ZOMBIE)
        panic("wait !zombie");

      // Found one.
      pid = p->pid;
      kfree(p->kstack);
      p->kstack = 0;
      freevm(p->pgdir);
      stateTransfer(&ptable.pLists.zombie, &ptable.pLists.zombieTail, ZOMBIE
          ,&ptable.pLists.free, &ptable.pLists.freeTail, UNUSED, p);
      p->pid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->killed = 0;
      release(&ptable.lock);
      return pid;
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifndef CS333_P3P4
// original xv6 scheduler. Use if CS333_P3P4 NOT defined.
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}

#else
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // check ready list looking for process to run.
    acquire(&ptable.lock);
    if(ptable.pLists.ready != 0) {
      p = ptable.pLists.ready;
      assertState(p, RUNNABLE);

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;
      switchuvm(p);

      stateTransfer(&ptable.pLists.ready, &ptable.pLists.readyTail, RUNNABLE
          ,&ptable.pLists.running, &ptable.pLists.runningTail, RUNNING, p);
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;

#ifdef CS333_P2
  proc->cpu_ticks_total += ticks - proc->cpu_ticks_in;
#endif

  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
#ifdef CS333_P3P4
  stateTransfer(&ptable.pLists.running, &ptable.pLists.runningTail, RUNNING
      ,&ptable.pLists.ready, &ptable.pLists.readyTail, RUNNABLE, proc);
#else
  proc->state = RUNNABLE;
#endif
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// 2016/12/28: ticklock removed from xv6. sleep() changed to
// accept a NULL lock to accommodate.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    if (lk) release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
#ifdef CS333_P3P4
  stateTransfer(&ptable.pLists.running, &ptable.pLists.runningTail, RUNNING
      ,&ptable.pLists.sleep, &ptable.pLists.sleepTail, SLEEPING, proc);
#else
  proc->state = SLEEPING;
#endif
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

//PAGEBREAK!
#ifndef CS333_P3P4
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  p = ptable.pLists.sleep;
  while(p) {
    if(p->chan == chan) {
      stateTransfer(&ptable.pLists.sleep, &ptable.pLists.sleepTail, SLEEPING
          ,&ptable.pLists.ready, &ptable.pLists.readyTail, RUNNABLE, p);
    }
    p = p->next;
  }
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifndef CS333_P3P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  acquire(&ptable.lock);
  if(killFromList(ptable.pLists.ready, pid) || killFromList(ptable.pLists.running, pid)
      || killFromList(ptable.pLists.sleep, pid)) {
    release(&ptable.lock);
    return 0;
  }

  release(&ptable.lock);
  return -1;
}
#endif

static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runnable",
  [RUNNING]   "running",
  [ZOMBIE]    "zombie"
};

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P2)
#define HEADER "\nPID\tName\tUID\tGID\tPPID\tElapsed CPU\tState\tSize\tPCs\n"
#elif defined(CS333_P1)
#define HEADER "\nElapsed\tPID\tState\tName\tPCs\n"
#else
#define HEADER ""
#endif

  cprintf(HEADER);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

#if defined(CS333_P2)
    procdumpP2(p, state);
#elif defined(CS333_P1)
    procdumpP1(p, state);
#else
    cprintf("%d %s %s", p->pid, state, p->name);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


#ifdef CS333_P3P4
static int
stateListAdd(struct proc** head, struct proc** tail, struct proc* p)
{
  if (*head == 0) {
    *head = p;
    *tail = p;
    p->next = 0;
  } else {
    (*tail)->next = p;
    *tail = (*tail)->next;
    (*tail)->next = 0;
  }

  return 0;
}

static int
stateListRemove(struct proc** head, struct proc** tail, struct proc* p)
{
  if (*head == 0 || *tail == 0 || p == 0) {
    return -1;
  }

  struct proc* current = *head;
  struct proc* previous = 0;

  if (current == p) {
    *head = (*head)->next;
    return 0;
  }

  while(current) {
    if (current == p) {
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found, hit eject.
  if (current == 0) {
    return -1;
  }

  // Process found. Set the appropriate next pointer.
  if (current == *tail) {
    *tail = previous;
    (*tail)->next = 0;
  } else {
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = 0;

  return 0;
}

static void
initProcessLists(void) {
  ptable.pLists.ready = 0;
  ptable.pLists.readyTail = 0;
  ptable.pLists.free = 0;
  ptable.pLists.freeTail = 0;
  ptable.pLists.sleep = 0;
  ptable.pLists.sleepTail = 0;
  ptable.pLists.zombie = 0;
  ptable.pLists.zombieTail = 0;
  ptable.pLists.running = 0;
  ptable.pLists.runningTail = 0;
  ptable.pLists.embryo = 0;
  ptable.pLists.embryoTail = 0;
}

static void
initFreeList(void) {
  if (!holding(&ptable.lock)) {
    panic("acquire the ptable lock before calling initFreeList\n");
  }

  struct proc* p;

  for (p = ptable.proc; p < ptable.proc + NPROC; ++p) {
    p->state = UNUSED;
    stateListAdd(&ptable.pLists.free, &ptable.pLists.freeTail, p);
  }
}

// checks ready, sleep and running lists for children
static int
findChildren(struct proc *parent) {
  int count;

  if(!holding(&ptable.lock))
    panic("findChildren ptable.lock");
  count = 0;
  count += numberChildren(ptable.pLists.ready, parent);
  count += numberChildren(ptable.pLists.sleep, parent);
  count += numberChildren(ptable.pLists.running, parent);
  return count;
}

static int
numberChildren(struct proc *head, struct proc *parent) {
  int count;
  struct proc *p;

  count = 0;
  p = head;
  while(p) {
    if(p->parent == parent)
      count++;
    p = p->next;
  }
  return count;
}

static int
killFromList(struct proc *head, int pid) {
  if(head == 0 || pid == 0) {
    return 0;
  }
  struct proc *p;

  p = head;
  while(p) {
    if(p->pid == pid) {
      p->killed = 1;
      // Wake proc from sleep if necessary.
      stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleepTail, p);
      p->state = RUNNABLE;
      stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, p);
      return 1;
    } 
    p = p->next;
  }
  return 0;
}

static void
assertState(struct proc *p, enum procstate state) {
  if(p->state != state)
    panic(states[state]);
}

static void
stateTransfer(struct proc **fromHead, struct proc **fromTail, enum procstate oldState
    ,struct proc **toHead, struct proc **toTail, enum procstate newState, struct proc *p)
{
  if(stateListRemove(fromHead, fromTail, p) < 0)
    panic("state list remove fail");
  assertState(p, oldState);
  p->state = newState;
  stateListAdd(toHead, toTail, p);
}
#endif

// Procdump() helper functions
#if defined(CS333_P2)
static void
procdumpP2(struct proc *p, char *state) {
  int elapsed = ticks - p->start_ticks;

  cprintf("%d\t%s\t%d\t%d\t%d\t",
      p->pid, p->name,
      p->uid, p->gid,
      p->parent ? p->parent->pid : p->pid);       // check if parent null
  cprintf("%d.%d ", elapsed/1000, elapsed%1000);
  cprintf("%d.%d\t%s\t%d\t",
      p->cpu_ticks_total/1000, p->cpu_ticks_total%1000,
      state, p->sz);
}

// Copies relevant process info from ptable into uprocs array.
// Returns the number of processes copied on success, and -1
// on failure.
int
ptablecopy(struct uproc *uprocs, int max) {
  struct proc *p;
  int n = 0;        // number of processes copied

  // check max isn't higher than maximum number of processes
  if(max > NPROC)
    return -1;

  for(p = ptable.proc; p < & ptable.proc[max]; p++){
    if(p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING) {
      uprocs[n].pid = p->pid;
      uprocs[n].uid = p->uid;
      uprocs[n].gid = p->gid;
      uprocs[n].ppid = p->parent ? p->parent->pid : p->pid;
      uprocs[n].elapsed_ticks = ticks - p->start_ticks;
      uprocs[n].CPU_total_ticks = p->cpu_ticks_total;
      uprocs[n].size = p->sz;
      strncpy(uprocs[n].name, p->name, STRMAX);
      strncpy(uprocs[n].state, states[p->state], STRMAX);

      n++;
    }
  }
  
  return n;
}
#elif defined(CS333_P1)
static void
procdumpP1(struct proc *p, char *state) {
  int elapsed = ticks - p->start_ticks;
  cprintf("%d.%d\t%d\t%s\t%s\t",
      elapsed/1000, elapsed%1000, p->pid, state, p->name);
}
#endif
