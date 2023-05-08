#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

int noPinQ[5] = {0};
struct proc *qproc[5][NPROC];

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
    //////////////////////
    p->TIME_CREATE = 0; // INITIALIZING CREATE TIME TO 0
    p->RUNS_NUMBER=0;   // INITIALIZING NUMBER OF RUNS TO 0
    p->TOTAL_TIME_RUN=0;// INITIALIZING TOTAL RUM TIME TO 0
    p->TIME_START=0;    // INITIALIZING START TIME TO 0
    p->TIME_SLEEP=0;    // INITIALIZING SLEEP TIME TO 0
    p->TIME_RUN=0;      // INITIALIZING RUN TIME OF EACH PROCESS TO 0
    p->TIME_EXIT=0;     // INITIALIZING EXIT TIME TO 0
    p->count_tick = 0;  // INITIALIZING COUNT OF TICK TO 0
    p->mask_no = 0;     // INITIALIZE MASK TO 0
    p->ticks = 0;       // INITIALIZING TICKS TO 0
    p->alarmistrue = 0; // INITIALIZE ALARMTRUE TO 0 -> NO ALARM INITIALLY
    //////////////////////
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

////////////////////////////////////////////////////////////////////////////////////////////////
void alocatedefault_PBC(struct proc *p)
{
    p->TIME_CREATE = ticks;     // CREATE TIME WILL BE JUST NO. OF TICKS
    p->TIME_RUN = 0;            // INITIALIZING RUN TIME TO 0
    p->TIME_SLEEP = 0;          // INITIALIZING SLEEP TIME TO 0
    p->TOTAL_TIME_RUN = 0;      // INITIALIZING TOTAL RUNNING TIME TO 0
    p->RUNS_NUMBER = 0;         // INITIALIZING NO. OF RUN TO 0
    p->priority = 60;           // MAKING PRIORITY AS 60 [DEFAULT] 
}
void aloc_tickets_LBS(struct proc *p)
{
   p->tickets = 1;             // THIS IS FOR LBSSSSS
}

void aloc_MLFQ(struct proc *p)
{
  p->qticks = ticks;
  p->qtrun = 0;
  for(int i=0;i<5;i++) p->QWaitTime[i] = 0;
  p->qNo = 0;
  p->ifq = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////
// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
 for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;

found:
    p->pid = allocpid();      
    p->state = USED;         
    /////////////////////////////
    aloc_tickets_LBS(p);       //
    alocatedefault_PBC(p);     //
    aloc_MLFQ(p);              //
    /////////////////////////////

  // Allocate a trapframe page.
  if (!(p->trapframe = (struct trapframe *)kalloc()))
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }
///// ALLOCATING MEMORY TO COPY OF TRAPFRAME /////////////
  if (!(p->copy_tf = (struct trapframe *)kalloc()))     //
  {                                                     //
    freeproc(p);                                        //
    release(&p->lock);                                  //
    return 0;                                           //
  }                                                     //
//////////////////////////////////////////////////////////
  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if ((p->trapframe))
    kfree((void *)p->trapframe);
  p->trapframe = 0;

  // FREEING MEMORY ALLOCATED TO COPY OF TRACE FRAME 
  if ((p->copy_tf)!=0)            //
    kfree((void *)p->copy_tf);    //
  p->copy_tf = 0;                 //
  //////////////////////////////////

  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->ifq = 0;
  p->state = UNUSED;
  p->tickets = 0; // +++++> INITIALIZING TICKETS TO 0
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;     // user program counter
  p->trapframe->sp = PGSIZE; // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  // #ifdef MLFQ
  // pushQ(p, 0);
  // #endif
  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  /////////////////////////////
   np->mask_no=p->mask_no;   // GIVE CHILD SAME MASK NO. AS OF PARENT 
  /////////////////////////////

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
//////////////////////////////
  np->tickets = p->tickets; // ++++++++++++++> FOR LBS
//////////////////////////////
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    if (pp->parent == p)
    {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

////////////////////////
p->TIME_EXIT = ticks; // SETTING EXIT TIME WHEN PROCESS EXITS
////////////////////////
  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.

int wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++)
    {
      if (pp->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE)
        {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                   sizeof(pp->xstate)) < 0)
          {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p))
    {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); // DOC: wait-sleep
  }
}
////////////////////////////// GINVEN ///////////////////////////////////////////////////////////
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
///////////////////////////////////////////////////////////////////////////////////////////////// 
// CALCULATING RUNNING TIME , SLEEP TIME AND TOTAL RUNNING TIME 
void update_time()
{
  struct proc* p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == RUNNING) 
    {
    #ifdef MLFQ
      p->QWaitTime[p->qNo]++;
      p->qtrun++;
    #endif
      p->TOTAL_TIME_RUN++;
      p->TIME_RUN++;              // incment the RUN TIME
    }
    if (p->state == SLEEPING)      // if the Process is sleeping
    {
      p->TIME_SLEEP++;
    }       // increment the sleepTime
    release(&p->lock); 
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////// FOR LBS ////////////////////////////////////////////////////////////////////////////
/// function to set ticket for currently running process

int setticket(int ticket)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->tickets = ticket;
  release(&p->lock); 
  return ticket;
}

int totaltickets()
{
  int tots = 0;
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++)
  {
    if(p->state == RUNNABLE) tots += p->tickets;
  }
  return tots;
}
int
do_rand(unsigned long *ctx)
{
/*
 * Compute x = (7^5 * x) mod (2^31 - 1)
 * without overflowing 31 bits:
 *      (2^31 - 1) = 127773 * (7^5) + 2836
 * From "Random number generators: good ones are hard to find",
 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
 * October 1988, p. 1195.
 */
    long hi, lo, x;

    /* Transform to [1, 0x7ffffffe] range. */
    x = (*ctx % 0x7ffffffe) + 1;
    hi = x / 127773;
    lo = x % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x < 0)
        x += 0x7fffffff;
    /* Transform to [0, 0x7ffffffd] range. */
    x--;
    *ctx = x;
    return (x);
}

int
rand(void)
{
    unsigned long int rand_next = 1;
    return (do_rand(&rand_next));
}

void pushQ(struct proc* p, int qidx)
{
  if(noPinQ[qidx] >= NPROC) return;

  for(int i=0; i < noPinQ[qidx]; i++)
  {
    if(qproc[qidx][i]->pid == p->pid) return;
  }

  qproc[qidx][noPinQ[qidx]] = p;
  p->qNo = qidx;
  noPinQ[qidx] += 1;
}

void popQ(struct proc *p, int qidx)
{
  if(noPinQ[qidx] == 0) panic("Empty queue");

  int k = 0, i = 0;
  for(;i < noPinQ[qidx]; i++)
  {
    if(qproc[qidx][i]->pid == p->pid)
    {
      k = 1;
      break;
    }
  }
  if(k == 0) return;

  qproc[qidx][i] = 0;
  for(int j = i; j < noPinQ[qidx]-1; j++)
  {
    qproc[qidx][j] = qproc[qidx][j+1];
  }
  noPinQ[qidx] -= 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for (;;)
  {
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef RR
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
#endif
    //////////////////////// FCFS //////////////////////////////////////////////////////////////////// 
#ifdef FCFS
    struct proc *firstProcess = 0;
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);              // LOCKING PROCESS
      if (p->state == RUNNABLE)
      {
        // FCFS scheduling 
        if (!firstProcess) // WHEN THERE IS NO PROCESS 
        {
          firstProcess = p;
        }
        if(firstProcess->TIME_CREATE > p->TIME_CREATE) // WHEN A PROCESS HAVE LESS CREATION TIME WILL GIVE FIRST PREFERRENCE 
        {
          firstProcess = p;
        }
      }
      release(&p->lock);             // RELEASING PROCESS
    }

    if (firstProcess)             // IF THERE IS PROCESS -> FIRST PROCESS
    {
       ///// REST SAME AS RR CODE //////
      acquire(&firstProcess->lock);  // lock the chosen process
      if (firstProcess->state == RUNNABLE)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        firstProcess->state = RUNNING;              // set running state
        c->proc = firstProcess;                     // set current process
        swtch(&c->context, &firstProcess->context); // switch to the process

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0; // no longer running
      }
      release(&firstProcess->lock); // release the lock
    }
#endif
///////////////////////////////////////  PBS ////////////////////////////////////////////////////////// 
#ifdef PBS
// printf("hello"); 
        struct proc *np = 0;   // creating a new proc 
        int nice = 5;         // default value of nice 
        int dp = 101;         // the process with the highest priority
        for (p = proc; p < &proc[NPROC]; p++) 
        {
            acquire(&p->lock); // lock the chosen process
            nice = 5;         // default value of nice 
            int total=p->TIME_RUN + p->TIME_SLEEP ; // total time  -> run + sleep 
            if (total>0)
            {
                nice = (( p->TIME_SLEEP) / (p->TIME_SLEEP + p->TIME_RUN)); // calculating the nice value
                nice =nice*10;
            }
                int t=100;
                if ((p->priority - nice + 5) < 100)
                {
                  t=(p->priority - nice + 5);
                }
                int process_dp = 0;
                if (t>0)
                {
                  process_dp=t;
                }
                // if process is runnable 
                if ((p->state == RUNNABLE)&&( np == 0 || (dp > process_dp) || (dp == process_dp && np->RUNS_NUMBER > p->RUNS_NUMBER) || ((dp == process_dp) && (np->RUNS_NUMBER == p->RUNS_NUMBER) && (np->TIME_CREATE > p->TIME_CREATE)))) 
                {
                    if (np>0)
                    {
                      release(&np->lock);   // release the lock of the previous process
                    }
                    dp = process_dp;         // set the dynamic priority
                    np = p;                  // set the process with the highest priority
                    continue;
                }
            release(&p->lock); // release the lock
        }
        if (np>0) 
        {                                        // if there is a process with the highest priority
            np->TIME_START = ticks;              // set the start time of the process with the highest priority
            np->RUNS_NUMBER++;                   // increase the number of runs of the process with the highest priority
            np->state = RUNNING;                 // set the process with the highest priority to running
            np->TIME_SLEEP = 0;                  // set the sleep time of the process with the highest priority to 0
            np->TIME_RUN = 0;                    // set the run time of the process with the highest priority to 0
            c->proc = np;                        // set the current process to the process with the highest priority
            
            swtch(&c->context, &np->context);    // switch to the process with the highest priority
                                                 // Process is done running for now.
                                                 // It should have changed its p->state before coming back.
            c->proc = 0;                         // no longer running
            release(&np->lock);                  // release the lock of the process with the highest priority
        }
#endif
///////////////////////////////////////  LBS ////////////////////////////////////////////////////////// 
#ifdef LBS
  int draw = 0;
  for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.

        int thresh = rand()%totaltickets();
        if((draw + p->tickets) < thresh)
        {
          draw += p->tickets;
          release(&p->lock);
          continue;
        }

        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    } 
  #endif
  #ifdef MLFQ
  // Aging of process
    for(p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if(p->state == RUNNABLE && ticks - p->qticks >= 30)
      {
        if(p->ifq == 1)
        {
          popQ(p, p->qNo);
          p->ifq = 0;
        }
        if(p->qNo > 0) p->qNo--;
        p->qticks = 0;
      }
      release(&p->lock);
    }

    for(p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if(p->state == RUNNABLE && p->ifq == 0)
      {
        pushQ(p, p->qNo);
        p->ifq = 1;
      }
      release(&p->lock);
    }

    for(int i=0; i < 5; i++)
    {
      int gg = 0;
      for(int j=0; j < noPinQ[i]; j++)
      {
        if(noPinQ[i] == 0) continue;
        if(p->state == RUNNABLE)
        {
          p = qproc[i][noPinQ[i]];
          p->ifq = 0;
          p->qticks = ticks;
          gg = 1;
          break;
        }
      }
      if(gg) break;
    }

    // if(!p) continue;

    acquire(&p->lock);
    p->state = RUNNING;
    p->qtrun = 0;
    p->qticks = ticks;
    c->proc = p;
    swtch(&c->context, &p->context);
    c->proc = 0;
    p->qticks = ticks;
    release(&p->lock);

  #endif
  }
///////////////////////////////////////////////////////////////////////////////////////////////////////////
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.

void sched(void) {
    int intena;
    struct proc *p = myproc();

    if (!holding(&p->lock))
        panic("sched p->lock");
    if (mycpu()->noff != 1)
        panic("sched locks");
    if (p->state == RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    swtch(&p->context, &mycpu()->context);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;

  // #ifdef MLFQ
  //   if(p->qNo < 4)

  // #endif

  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); // DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p != myproc())
    {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->chan == chan)
      {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
    // printf("838");
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [USED] "used",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
      
    printf("%d %s %s", p->pid, state, p->name);

    #ifdef MLFQ
    printf("%d %d %s %d %d %d %d %d %d %d", p->pid, p->qNo, state, p->TOTAL_TIME_RUN, ticks - p->qticks, p->QWaitTime[0], p->QWaitTime[1], p->QWaitTime[2], p->QWaitTime[3], p->QWaitTime[4]);
    #endif

    printf("\n");
  }
}
////////////////////////////// PBS /////////////////////////////////
int set_priority(int np, int pid)                                 //
{                                                                 //
    struct proc *p;                                               //
    int t = -1;                                                   //
    for (p = proc; p < &proc[NPROC]; p++) {                       //
        acquire(&p->lock);              // locking the process    //
        if (pid == p->pid)              // if the process is found//
         {                                                        //
            t = p->priority;            // saving the old priority//
            p->priority = np;           // seting the new priority//
            release(&p->lock);          // releasing the process  //
            if (t > np)                 // if the np is lower     //
            {                                                     //
                yield();                // yield the CPU          //
            }                                                     //
                                                                  //
            break;                                                //
         }                                                        //
        release(&p->lock);              // release the process    //
    }                                                             //
    return t;                                                     //
}                                                                 //
////////////////////////////////////////////////////////////////////

int
waitx(uint64 addr, uint* wtime, uint* rtime)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          *rtime = np->TOTAL_TIME_RUN;
          *wtime = np->TIME_EXIT - np->TIME_CREATE - np->TOTAL_TIME_RUN;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

