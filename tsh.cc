// 
// tsh - A tiny shell program with job control
// 
// <Amber Womack , amwo9446>
// to open geany tshcc &

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>

#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions
//

static char prompt[] = "tsh> ";
int verbose = 0;

//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
// 

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

//
// main - The shell's main routine 
//
int main(int argc, char **argv) 
{
  int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
  dup2(1, 2);

  /* Parse the command line */
  char c;
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             // print help message
      usage();
      break;
    case 'v':             // emit additional diagnostic info
      verbose = 1;
      break;
    case 'p':             // don't print a prompt
      emit_prompt = 0;  // handy for automatic testing
      break;
    default:
      usage();
    }
  }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
  Signal(SIGINT,  sigint_handler);   // ctrl-c
  Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
  Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
  Signal(SIGQUIT, sigquit_handler); 

  //
  // Initialize the job list
  //
  initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
  for(;;) {
    //
    // Read command line
    //
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }

    char cmdline[MAXLINE];

    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
      app_error("fgets error");
    }
    //
    // End of file? (did user type ctrl-d?)
    //
    if (feof(stdin)) {
      fflush(stdout);
      exit(0);
    }

    //
    // Evaluate command line
    //
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); //control never reaches here
}
  
/////////////////////////////////////////////////////////////////////////////make test01
//
// eval - Evaluate the command line that the user has just typed in
// 
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//
void eval(char *cmdline) 
{
  /* Parse command line */
  //
  // The 'argv' vector is filled in by the parseline
  // routine below. It provides the arguments needed
  // for the execve() routine, which you'll need to
  // use below to launch a process.
  //
  char *argv[MAXARGS];
	
  //
  // The 'bg' variable is TRUE if the job should run
  // in background mode or FALSE if it should run in FG
  //
  //initializing the pid variable as a pid_t
  pid_t pid;
  //initializing the mask as a sigset_t
  sigset_t mask;
  //sig empty set means nothing is there. mask will contain a set of signals ?????????????
  sigemptyset(&mask);
  //start adding child signal sets to mask, 
  sigaddset(&mask, SIGCHLD);
  
  int bg = parseline(cmdline, argv); 
  if (argv[0] == NULL)  
    return;   /* ignore empty lines */
    
  if(!builtin_cmd(argv)) //creating an if statement for all the non builtin commands
  {
	  sigprocmask(SIG_BLOCK, &mask, 0);
	  //set pid to equal fork, and if fork and pid both equal 0, then its a child process
	  if((pid = fork()) == 0){
		  setpgid(0,0); //set the process group id for the children to be the same, when pgid is 0,0 will return the address of the children.
		  //exec , found command in book. found that built in command isn't for child process 
		  if(execvp(argv[0], argv) < 0) {
			  printf("%s: Command not found. /n", argv[0]);
			  exit(0);
		        }
		}
		else{ //begin dealing with parent process. 
			if(bg == 1){		//fcn that adds to the bg jobs list
				addjob(jobs, pid, BG, cmdline);
					//prints out where it was in the background list, name of command sent in. 
					//skeleton of what its supposed to print. 
				printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
				sigprocmask(SIG_UNBLOCK, &mask, 0);
			}
			else //for the fg job in the parent process. 
			{
				addjob(jobs, pid, FG, cmdline);
				sigprocmask(SIG_UNBLOCK, &mask, 0); //-------------------------
				waitfg(pid);
				
			}
		}
  }

  return;
}


/////////////////////////////////////////////////////////////////////////////make test02
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need 
// to use the argv array as well to look for a job number.
//

int builtin_cmd(char **argv) 
{
  string cmd(argv[0]);
  
  if(cmd == "quit")
  {                           //this is a built in command so that if the user types quit, it will itll exit the shell
	exit(0);                   //exit zero will exit the shell is there is no error. 
  }
  if(cmd == "jobs")
  {
	listjobs(jobs);
	return 1;  
	}
	if(cmd == "bg" || cmd == "fg") //----------------------------
	{
		do_bgfg(argv);
		return 1;
	}
  return 0;     /* not a builtin command */
}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
//
void do_bgfg(char **argv) 
{
  struct job_t *jobp=NULL;
    
  /* Ignore command if no argument */
  if (argv[1] == NULL) {
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }
    
  /* Parse the required PID or %JID arg */
  if (isdigit(argv[1][0])) {
    pid_t pid = atoi(argv[1]);
    if (!(jobp = getjobpid(jobs, pid))) {
      printf("(%d): No such process\n", pid);
      return;
    }
  }
  else if (argv[1][0] == '%') {
    int jid = atoi(&argv[1][1]);
    if (!(jobp = getjobjid(jobs, jid))) {
      printf("%s: No such job\n", argv[1]);
      return;
    }
  }	    
  else {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }

  //
  // You need to complete rest. At this point,
  // the variable 'jobp' is the job pointer
  // for the job ID specified as an argument.
  //
  // Your actions will depend on the specified command
  // so we've converted argv[0] to a string (cmd) for
  // your benefit.
  //
  string cmd(argv[0]);
  //goal: Make a stop process, make an if statement for BG and and if statement for FG
  if (cmd == "bg"){
	  pid_t t_pid = jobp -> pid;
	   if(kill(-t_pid , SIGCONT) == -1){
		   printf("Something messed up in kill process");
	   }
	   jobp->state = BG;
	   printf("[%d] (%d) %s" , jobp->jid, jobp->pid, jobp->cmdline);
	   
  
  }
  
  if(cmd == "fg"){
	  pid_t t_pid = jobp -> pid;
	  if(kill(-t_pid , SIGCONT) == -1){
		  printf("Something messed up in kill process");
	  }
	  jobp->state = FG;
	  waitfg(t_pid);
		   
  }




  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
//
void waitfg(pid_t pid)
{
	while (getjobpid(jobs, pid) != NULL && getjobpid(jobs, pid) -> state == FG)
		sleep(1);
  return;
}
//returns 0 once there is no fg job
/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//     a child job terminates (becomes a zombie), or stops because it
//     received a SIGSTOP or SIGTSTP signal. The handler reaps all
//     available zombie children, but doesn't wait for any other
//     currently running children to terminate.  Where moest of code is done. 
// output child is terminate here. from output the status. 
//
void sigchld_handler(int sig) 
{
 int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0)    {
        if (WIFSIGNALED(status) || WIFEXITED(status)) {
            //If the process is actually terminated, bookkeep it.  
            deletejob(jobs,pid);
        }
    }
    if (pid < 0){
        int child = ECHILD; 
        if (errno != child) {
            printf("waitpid error: %s\n", strerror(errno));
        }
   }
}
/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenver the
//    user types ctrl-c at the keyboard.  Catch it and send it along
//    to the foreground job.  ALL you need to do is send a sendint sig to the foreground. 
// dont call signal handler from another fcn. 
//
void sigint_handler(int sig) 

	{
	//initialize a process id from the forground pid off the jobs being process, so that it can make a list of what is happening. 
	pid_t pid = fgpid(jobs);
	//creating a pointer to the job in the fg job list. based off the pid passed in. 
	job_t* job = getjobpid(jobs, pid);
	//statement occurs if the pid DNE in the job list, will print no job. 
	if(!pid)
	{
		printf("No foreground job \n");
		//no fg job, do nothing
		return;
	}
	//if returns a -1, nothing to process and trying to terminate, should create an error.
	if(kill(-pid, SIGINT) == -1)
	{
			printf("Error in terminating foreground process \n");
			return;
	}
	
	printf("Job [%d] (%d) terminated by signal %d", job->jid, job->pid, sig);
	//we sent a signal to terminate the process, delete job will delete. 
	deletejob(jobs, pid);
	return;
}
  

/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//     the user types ctrl-z at the keyboard. Catch it and suspend the
//     foreground job by sending it a SIGTSTP.  
//
void sigtstp_handler(int sig) 
{
	 pid_t pid = fgpid(jobs);
	job_t* job = getjobpid(jobs, pid);
	if(!pid){
		printf("No forground job\n");
		return; //no Foreground job, do nothing
	}
	if(kill(-pid , SIGTSTP) == -1){
		printf("Error in terminating foreground process \n");
		return;
	}
	job->state = ST;
	printf("Job [%d] (%d) stopped by signal %d\n" , job->jid, job->pid, sig);
	
  return;
}

/*********************
 * End signal handlers
 *********************/




