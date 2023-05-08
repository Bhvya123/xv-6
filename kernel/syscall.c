#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz) // both tests needed, in case of overflow
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
///////////////////////////////////////
extern uint64 sys_trace(void);       // ADDING STRACE
extern uint64 sys_sigalarm(void);    // ADDING SIGALARM
extern uint64 sys_sigreturn(void);   // ADDING SIGRETURN
extern uint64 sys_set_priority(void);//
extern uint64 sys_settickets(void);  //
extern uint64 sys_waitx(void);       //
///////////////////////////////////////

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
/////////////////////////////////////
[SYS_trace]   sys_trace,           // ADDING STRACE 
[SYS_sigalarm] sys_sigalarm,       // ADDING SIGALARM
[SYS_sigreturn] sys_sigreturn,     // ADDING SIGRETURN
[SYS_set_priority]sys_set_priority,//
[SYS_settickets] sys_settickets,   //
[SYS_waitx]      sys_waitx         //
/////////////////////////////////////

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
char* sysnames[] = {"NIL","fork","exit","wait","pipe","read","kill","exec","fstat","chdir","dup","getpid",                          //
                    "sbrk","sleep","uptime","open","write","mknod","unlink","link","mkdir","close","trace","sigalarm","sigreturn","set_priority"}; //
                                                                                                                                    //
int sysargc[] = {0,0,1,1,1,3,1,2,2,1,1,0,1,1,0,2,3,3,1,2,1,1,1,2,0,2};                                                                //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// a0 -> command index after trace and return value after exec so we need to store value of a0 temporary after trace and before exec//
// a7 -> system call index                                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
syscall(void)
{
  int num;
  struct proc *p = myproc();
  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) 
  {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int com_index =p->trapframe->a0;   // storing a0 -> command index which will be over written by return value when exec is executed//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    p->trapframe->a0 = syscalls[num]();

////////// code for starce //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if(p->mask_no != 0)                         
    {                                           
       if(1<<num & p->mask_no)                  
       {                                        
          printf("%d: ",p->pid);                // printing pid
          printf("syscall %s ",sysnames[num]);  // printing syscall name
          int no_of_arguments = sysargc[num];   // storing no. of arguments of a command 
          int ret_val=p->trapframe->a0;         // storing ret_value of a system call
        if(no_of_arguments == 0)                // no argument 
        {                                       
          printf("() -> %d\n",ret_val);         // printing only return value
        }                                       
        else if(no_of_arguments == 1)           // one argument 
        {                                       
          printf("(%d) -> %d\n",com_index,p->trapframe->a0); 
        }                                       
        else if(no_of_arguments == 2)           // two argument 
        {                                       
          printf("(%d %d) -> %d\n",com_index,p->trapframe->a1,ret_val);
        }                                       
        else if(no_of_arguments == 3)           // three argument
        {                                       
          printf("(%d %d %d) -> %d\n",com_index,p->trapframe->a1,p->trapframe->a2,ret_val);
        }
       }
    }
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
