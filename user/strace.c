#include "user/user.h"

int main(int argc,char ** argv)   
{
    int pid=0;
    // checking for no. of arguments condition
    if(argc != 3)
    {
        printf("Usage strace <mask> <command [args]> \n");
        exit(0);
    }
    else if (trace(atoi(argv[1])) < 0) 
    {
        printf("UNABLE TO EXECUTE TRAC \n");
        exit(0);
    }
    else
    {
         pid=fork();
         if(pid==0)                   //checking for child process
         {
            int mask = atoi(argv[1]); //getting mask -> 1st argument 
            trace(mask);              //syscall trace 
            exec(argv[2],&argv[2]);   //predefined -> similar to execvp 
         }
         wait(0);                     //waiting for child process to be executed
    }
    return 0;                         
}