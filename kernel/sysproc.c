#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
/////////////////////////////////////////// MADE BY KABIR ////////////////////////////////////////////////////
uint64 
sys_trace(void)
{
////////////////////////////////
  int mask_no;                //
  argint(0,&mask_no);         //
  myproc()->mask_no=mask_no;  //
  return mask_no;             //
////////////////////////////////
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64
sys_sigalarm(void)
{
//////////////////////////////////////////////
  myproc()->alarmistrue=1;                  // setting alarm as true 
  int ticks;                                // making ticks 
  argint(0,&ticks);                         // getting tick provided via argument 
  myproc()->ticks = ticks;                  // storing no. of ticks
  myproc()->count_tick=0;                   // initializing tick count 
  struct proc *p = myproc();                // 
  uint64 handler;                           // making handler to get thr handler 
  handler = p->trapframe->a1;               // extracting first argument that will be handler  
  myproc()->handler = handler;              // assigning handler        
  return 1;                                 // returning 1
  ////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64                                                                                                                                                            //
sys_sigreturn(void)                                                                                                                                               //
{                                                                                                                                                                 //
  struct proc* p = myproc();                                                                                                                                      //
  // saving the previous occured process state in copy_tf //                                                                                                      //
  p->copy_tf->kernel_trap = p->trapframe->kernel_trap;    //                                                                                                      //
  p->copy_tf->kernel_sp = p->trapframe->kernel_sp;        //                                                                                                      //
  p->copy_tf->kernel_satp = p->trapframe->kernel_satp;    //                                                                                                      //
  p->copy_tf->kernel_hartid = p->trapframe->kernel_hartid;//                                                                                                      //
  //////////////////////////////////////////////////////////                                                                                                      //
  //  NOW RESTORING //                                                                                                                                            //
  /////////////////////////////////////////////                                                                                                                   //
  *(p->trapframe) = *(p->copy_tf);           // making previous process to be continued / RESTORING                                                               //
  p->alarmistrue = 1;                        // setting alarm value as true becoz if the handler will took more time than the ticks so the alarm will be set to 1 //
  return p->trapframe->a0;                   // returning argument 0 -> a0                                                                                        //
  /////////////////////////////////////////////                                                                                                                   //
}                                                                                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//syscall will only change priority if it is in range [0,100]
//returns -1 in error condition
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64
sys_set_priority()
{
int priority, pid;
int oldpriority = 101;      
argint(0, &priority);        // taking priority 
argint(1, &pid);             // taking pid 
if(priority < 0 ||  pid< 0)  // checking for
    return -1;

for(struct proc *p = proc; p < &proc[NPROC]; p++)
{
    acquire(&p->lock);

    if(p->pid == pid && priority >= 0 && priority <= 100)
    {
    p->TIME_SLEEP = 0;
    p->TIME_RUN = 0;
    oldpriority = p->priority;
    p->priority = priority;
    }

    release(&p->lock);
}
if(oldpriority > priority)
yield();
return oldpriority;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// system call to change tickets in currently running process. 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern int setticket(int);


uint64
sys_settickets(void)
{
  int ticket;
  argint(0, &ticket);
  if(ticket < 0 ) return -1;
  return setticket(ticket);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////
uint64
sys_waitx(void)
{
  uint64 addr, addr1, addr2;
  uint TIME_WAIT, TIME_RUN;
  argaddr(0, &addr);
  argaddr(1, &addr1); // user virtual memory
  argaddr(2, &addr2);
  int ret = waitx(addr, &TIME_WAIT, &TIME_RUN);
  struct proc* p = myproc();
  if (copyout(p->pagetable, addr1,(char*)&TIME_WAIT, sizeof(int)) < 0)
    return -1;
  if (copyout(p->pagetable, addr2,(char*)&TIME_RUN, sizeof(int)) < 0)
    return -1;
  return ret;
}
//////////////////////////////////
