# CIS-415---RTOS-Practice

This is a project I worked on during Spring 2017.  A complete description of the project specifications are in P1Handout.pdf.

## Running the Project 
This should work on any linux operating system.  Use the Makefile to create the executable "uspsv3".
Invoke the executable on the command line using:

- ./uspsv3 [--quantum=(msec)] [workload_file]

Where msec is desired quantum expressed in milliseconds and the workload_file is a file containing a list of bash commands.
If you do not specify quantum it will search for it in your bash environment variables.  If it is not there either it will exit.
If you do not specify a workload_file it will read from stdin.

You can view the .txt files in this directory to see example workload files.

## What it does
This program will read the workload file and fork a new process for every line in the file.
Each child process will then execute the command it was given and then terminate once it has finished.

The original process will act as a Round Robin scheduler.  Each process is paused upon being created,
and the original process will let each process execute for <quantum> milliseconds before it will interrupt the
process and put it back into the ready queue.  Then it will select the next process in the ready queue and let
that process start executing.

Once all child processes have terminated, the original process will also terminate.

## Other
You can compile cpubound.c and iobound.c to create some other executables to see the behavior of the scheduler.
- gcc -o cpubound cpubound.c
- gcc -o iobound iobound.c

Each of these executables will do meaningless busy work for about 1 minute each.

So, if you run 
- ./uspsv3 work2.txt --quantum=5000 
You launch 6 of these programs, and then you can view the behavior of the scheduler by running the linux "top" command.
There you will see that only one of these programs is executing at a time.  After about 6 minutes, they will all be done
and your main program will terminate.
