#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//  you may need other standard header files


//  myscheduler (v1.0)
//  Compile with:  cc -std=c11 -Wall -Werror -o myscheduler myscheduler.c


//  THESE CONSTANTS DEFINE THE MAXIMUM SIZE OF sysconfig AND command DETAILS
//  THAT YOUR PROGRAM NEEDS TO SUPPORT.  YOU'LL REQUIRE THESE //  CONSTANTS
//  WHEN DEFINING THE MAXIMUM SIZES OF ANY REQUIRED DATA STRUCTURES.

#define MAX_DEVICES                     4
#define MAX_DEVICE_NAME                 20
#define MAX_COMMANDS                    10
#define MAX_COMMAND_NAME                20
#define MAX_SYSCALLS_PER_PROCESS        40
#define MAX_RUNNING_PROCESSES           50

//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

#define DEFAULT_TIME_QUANTUM            100

#define TIME_CONTEXT_SWITCH             5
#define TIME_CORE_STATE_TRANSITIONS     10
#define TIME_ACQUIRE_BUS                20


//  ----------------------------------------------------------------------

#define CHAR_COMMENT                    '#'

// Structure for storing device information
struct {
    char devicename[MAX_DEVICE_NAME];
    char readspeed[20];
    char writespeed[20];
} devices[MAX_DEVICES];

// Structure for storing command information
struct Syscall {
    char time[MAX_COMMAND_NAME];
    char syscall_name[MAX_COMMAND_NAME];
    char device_used[MAX_COMMAND_NAME];
    char storage[MAX_COMMAND_NAME];
};

// Structure for storing syscall information
struct {
    char command_name[MAX_SYSCALLS_PER_PROCESS];
	struct Syscall syscalls[]; 
} commands[MAX_COMMANDS];

struct Process {
    struct Command command;
    int current_syscall_index;
    int remaining_time;
    int blocked_device_index;
};

int current_time = 0;
int num_devices = 0;
int num_syscalls = 0;
int num_commands = 0;

void read_sysconfig(char argv0[], char filename[])
{
	FILE *fp;
    char line[1024];
    fp = fopen(filename, "r");

   	if (fp == NULL) {
        	perror("Error opening file");
        	exit(EXIT_FAILURE);
	}

	while (fgets(line, sizeof(line), fp) != NULL) {

		char first_column[20];

		if (strncmp(line, "device", 6) == 0) {
			sscanf(line, "%s %s %s %s", first_column, devices[num_devices].devicename, devices[num_devices].readspeed, devices[num_devices].writespeed); 
            num_devices++; 
		}

		else if (strncmp(line, "timequantum", 11) == 0) {
			sscanf(line, "%d", current_time);
		}

		else {
			continue;
		}
	}
	fclose(fp);
}

void read_commands(char argv0[], char filename[])
{
	FILE *fp;
   	char line[1024];
	fp = fopen(filename, "r");
    	
	if (fp == NULL) {
	     	perror("Error opening file");
        	exit(EXIT_FAILURE);
    	}

	while ((fgets(line, sizeof(line), fp) != NULL)) {

		if (line[0] == '\t') {
			struct Syscall current_syscall;
			sscanf(line, "%s %s %s %s", current_syscall.time, current_syscall.syscall_name, current_syscall.device_used, current_syscall.storage);
			
			//split time by removing usecs
			//split storage by removing B

			commands[num_commands].syscalls[num_syscalls] = current_syscall;
			num_syscalls++;
		}

		else if (line[0] == '#' || line[0] == ' ') {
					continue;
			}

		else {
            sscanf(line, "%s", commands[num_commands].command_name);
            num_commands++;
		}
	}

    fclose(fp);
}

//  ----------------------------------------------------------------------

void execute_commands(void)
{
	int current_command_index = 0;
    struct Process running_process;
    running_process.current_syscall_index = 0;
    running_process.remaining_time = 0;
    running_process.blocked_device_index = -1;

    while (current_command_index < num_commands || running_process.remaining_time > 0) {
        // Check if there's a new command to start
        if (running_process.remaining_time == 0 && current_command_index < num_commands) {
            running_process.command = commands[current_command_index];
            running_process.current_syscall_index = 0;
            running_process.remaining_time = running_process.command.syscalls[0].time;
            current_command_index++;
        }

        // Execute the current syscall of the running process
        if (running_process.remaining_time > 0) {
            execute_commands(&running_process);
            running_process.remaining_time--;
        }

        // Update time
        current_time++;

        // Implement other state transitions as needed
    }
}

//  ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
//  ENSURE THAT WE HAVE THE CORRECT NUMBER OF COMMAND-LINE ARGUMENTS
    if(argc != 3) {
        printf("Usage: %s sysconfig-file command-file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

//  READ THE SYSTEM CONFIGURATION FILE
    read_sysconfig(argv[0], argv[1]);

//  READ THE COMMAND FILE
    read_commands(argv[0], argv[2]);

//  EXECUTE COMMANDS, STARTING AT FIRST IN command-file, UNTIL NONE REMAIN
    execute_commands();

//  PRINT THE PROGRAM'S RESULTS
    printf("measurements  %d  %d\n", current_time, 0);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4
