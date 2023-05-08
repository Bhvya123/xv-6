#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[])
{
// CHECKING IF THE SCHEDULING IS PRIORITY BASED OR NOT 
#ifdef PBS
    if(argc != 3 ) 
    {
        printf("WRONG USAGE OF COMMAND");
        exit(0);
    }
    else if(argv[1][0] < '0' || argv[1][0] > '9')
    {
        printf("ARGUMENT OF %s NOT VALID");
        exit(0);
    }
    int a1,a2,ret;
    a1=atoi(argv[1]);
    a2=atoi(argv[2]);
    ret = set_priority(a1,a2); // calling syscall defined by us  
    // return -1 -> when priority is not been set
    if (ret < 0) 
    {
        printf( "UNABLE TO SET PRIORITY\n");
        exit(0);
    } 
    // return 101 -> that is not a valid priority as priority <=100 and this implies sycall was not able to get pid of the process 
    if (ret == 101)  
    {
        printf("UNABLE TO FIND PID\n");
        exit(0);
    }
#endif
     return(0);
}