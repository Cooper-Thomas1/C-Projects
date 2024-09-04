#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>

//  CITS2002 Project 1 2023
//  Student1:   23723986   Cooper Thomas
//  Student2:   23620112   Hugo Smith

#define MAX_DEVICES                     4
#define MAX_DEVICE_NAME                 20
#define MAX_COMMANDS                    10
#define MAX_COMMAND_NAME                20
#define MAX_SYSCALLS_PER_PROCESS        40
#define MAX_RUNNING_PROCESSES           50

#define DEFAULT_TIME_QUANTUM            100
#define CHAR_COMMENT                    '#'

#define TIME_CONTEXT_SWITCH             5           //need to comb through and replace any transitions with these.
#define TIME_CORE_STATE_TRANSITIONS     10
#define TIME_ACQUIRE_BUS                20

//  ----------------------------------------------------------------------


// Structures for storing device information
typedef struct {
    char device_name[MAX_DEVICE_NAME];
    char readspeed[1024];
    char writespeed[1024];
    int readspeed_ranking;
} Device;

Device devices[MAX_DEVICES];

// Structures for storing command information
typedef struct {				
    char time[MAX_COMMAND_NAME];
    char syscall_name[MAX_COMMAND_NAME];
    char arg3[MAX_COMMAND_NAME];
    char arg4[MAX_COMMAND_NAME];
} SyscallType;	

typedef struct {
    char command_name[MAX_COMMAND_NAME];
    SyscallType syscalls[MAX_SYSCALLS_PER_PROCESS]; 
    int num_syscalls;		
} Command;

Command commands[MAX_COMMANDS];

// Global variables
int num_commands = 0;
int num_devices = 0;
int total_time = 0;
int cpu_time = 0;
int pid_counter = 0;
int num_processes = 0;
int time_quantum;


// Trims trailing letters from a number
void remove_letters(char *str)
{
    if (!isalpha(str[0])) {
        int len = strlen(str);
        for (int i = 0; i < len; i++) {
            if (!isdigit(str[i])) {
                str[i] = '\0';
            }
        }
    }
}

// Reads and saves sysconfig file information
void read_sysconfig(char filename[])
{							
    FILE *fp;
    char line[1024];

    fp = fopen(filename, "r");

    if (fp == NULL) {
            perror("Error opening file\n");
            exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
								
        if (strncmp(line, "device", 6) == 0) {
            sscanf(line, "%*s %s %s %s", devices[num_devices].device_name, devices[num_devices].readspeed, devices[num_devices].writespeed); 

            remove_letters(devices[num_devices].readspeed);
            remove_letters(devices[num_devices].writespeed);

            num_devices++; 
        }							

        else if (strncmp(line, "timequantum", 11) == 0) {
            char temp[20];
            sscanf(line, "%*s %s", temp);
            time_quantum = atoi(temp);  

        }
    }
    fclose(fp);

    // Ranks devices on their readspeeds
    for (int i = 0; i < num_devices; i++) {
        devices[i].readspeed_ranking = 1;

        for (int j = 0; j < num_devices; j++) {
            if (i != j) {
                if (strcmp(devices[i].readspeed, devices[j].readspeed) < 0) {
                    devices[i].readspeed_ranking++;
                }
            }
        }
    }
}	


// Reads and saves command file
void read_commands(char filename[])
{
    FILE *fp;
    char line[1024];

    fp = fopen(filename, "r");  
    if (fp == NULL) {
            perror("Error opening file\n");
            exit(EXIT_FAILURE);
        }
    
    int num_syscalls = 0;

    while ((fgets(line, sizeof(line), fp) != NULL)) {

        if (line[0] == CHAR_COMMENT) {
                    continue;
            }

        else if (line[0] == '\t') {		
            
            SyscallType process;
            process.arg3[0] = '\0';
            process.arg4[0] = '\0';
            
            sscanf(line, "%s %s %s %s", process.time, process.syscall_name, process.arg3, process.arg4);

            remove_letters(process.time);
            remove_letters(process.arg3);
            remove_letters(process.arg4);

            commands[num_commands - 1].syscalls[num_syscalls] = process;
            commands[num_commands - 1].num_syscalls++;
            num_syscalls++;
        }

        else {
            sscanf(line, "%s", commands[num_commands].command_name);
			num_syscalls = 0;
            num_commands++;
        }
    }

    fclose(fp);
}


//  ----------------------------------------------------------------------

// Structures for storing process information
typedef enum {
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    WAITING,
    READING,
    WRITING,
    EXIT
} ProcessState;

typedef enum {
    OCCUPIED,
    UNOCCUPIED
} DataBusState;

// Process Control Block (PCB) structure
typedef struct {
    int pid; 
    
    ProcessState state;    
    int remaining_transition_time;
    int target_sleep_time;

    int command_index;
    int current_syscall_index;
    int num_syscalls; 
    int remaining_syscall_time; 

    int num_children;
    int child_pids[MAX_RUNNING_PROCESSES];
    int parent_pid;

    int target_transfer_time;
} PCB;

typedef struct {
    DataBusState state;
    int target_time;
} DataBus;

typedef struct {
    PCB queue[MAX_RUNNING_PROCESSES];  
    int front;                  
    int rear;                   
    int size;                   
} Queue; 


// Initialises PCB struct
void create_PCB(PCB *pcb, Command *command) 
{
    for (int i = 0; i < num_commands; i++) {
        if (&commands[i] == command) {
            pcb->command_index = i;
            break;
        }
    } 

    num_processes++;

    pcb->pid = pid_counter;
    pcb->state = NEW;
    pcb->num_syscalls = command->num_syscalls;
    pcb->current_syscall_index = 0;
    pcb->remaining_syscall_time = atoi(command->syscalls[0].time);

    pcb->parent_pid = -1;

    pid_counter++;
}

// Advances PCB current syscall index
void advance_syscall(PCB *pcb)
{
    pcb->current_syscall_index++;

    int previous_syscall_time = atoi(commands[pcb->command_index].syscalls[pcb->current_syscall_index - 1].time);
    int current_cumulative_time = atoi(commands[pcb->command_index].syscalls[pcb->current_syscall_index].time);

    pcb->remaining_syscall_time = current_cumulative_time - previous_syscall_time;
}

PCB *dequeue(Queue *queue) {

    if (queue->size == 0) {
        printf("Error: cannot dequeue an empty queue\n");
    }

    PCB *next_pcb = &(queue->queue[queue->front]);

    queue->front = (queue->front + 1);
    if(queue->front > MAX_RUNNING_PROCESSES) {
        queue->front = 0;
    }
    queue->size--;

    return next_pcb;
}

void enqueue(Queue *queue, PCB *pcb) 
{
    PCB *pcb_copy = (PCB *)malloc(sizeof(PCB));
    if (pcb_copy == NULL) {
        fprintf(stderr, "Error: Memory allocation for pcb_copy failed.\n");
        exit(EXIT_FAILURE);
    }

	// Copy memory and then add it to the queue
    memcpy(pcb_copy, pcb, sizeof(PCB));

    queue->queue[queue->rear] = *pcb_copy;
    queue->rear = (queue->rear + 1);
    queue->size++;
}

// Returns true if PCB has active children
int check_for_children(Queue *active_children, int child_pids[], int child_pids_size)
{
    for (int i = 0; i < child_pids_size; i++) {
        for(int j = 0; j < active_children->size; j++) {
            if (child_pids[i] == active_children->queue[j].pid) {
                return 1;
            }
        }
    }

    return 0;
}

// Finds and removes a PCB from a queue
void remove_pcb_from_queue(Queue *queue, PCB *pcb)
{
    int found_index = -1;

        for (int i = 0; i < queue->size; i++) {
            if (queue->queue[i].pid == pcb->pid) {
                found_index = i;
            }
        }

    if (found_index != -1) {
        // Shift the elements in the queue to remove the found process
        for (int i = found_index; i < queue->size - 1; i++) {
            queue->queue[i] = queue->queue[i + 1];
        }
        queue->rear--;
        queue->size--;
    }
}

// Returns the next waking PCB
PCB *find_woke(Queue *sleeping)
{
    PCB *woken_process = NULL;
    int found_index = -1;

    while (woken_process == NULL){
        for (int i = 0; i < sleeping->size; i++) {
    
        printf("%d+ idle\n", total_time);

            total_time++;

            if (sleeping->queue[i].target_sleep_time <= total_time) {
                printf("%d+ pid%d.SLEEPING -> READY, transition takes 10usecs\n", total_time, sleeping->queue[i].pid);
                found_index = i;  // Store the index of the found process
                woken_process = &(sleeping->queue[i]);
            } 
        }
    }

    if (found_index != -1) {
        // Shift the elements in the queue to remove the found process
        for (int i = found_index; i < sleeping->size - 1; i++) {
            sleeping->queue[i] = sleeping->queue[i + 1];
        }
        sleeping->rear--;
        sleeping->size--;

        woken_process->state = READY;
        woken_process->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS;
    } else {
        total_time++;  // Increment total_time if no process is woken
        printf("%d+ CPU idle\n", total_time);
    }

    return woken_process;
}

// Returns next PCB waiting for all children to finish
PCB *find_waiting(Queue *waiting, Queue *active_children)
{
    PCB *next_waiting_process;
    int found_index = -1;

    for (int i = 0; i < waiting->size; i++) {
        if(check_for_children(active_children, waiting->queue[i].child_pids, waiting->queue[i].num_children) == 0) {
            found_index = i;
            break;
        }
    }

    if (found_index == -1) {
        printf("%d+ all parents still waiting for their children to finish\n", total_time);
        return NULL;
    } else {
        next_waiting_process = &waiting->queue[found_index];
        next_waiting_process->state = READY;
        next_waiting_process->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS;
        printf("pid%d.WAITING -> READY", next_waiting_process->pid);

        remove_pcb_from_queue(waiting, next_waiting_process);

        return next_waiting_process;
    }
}


int calculate_transfer_time(int speed, int size)
{
    double transfer_time = (double)size / (double)(speed / 1000000);
    int rounded_time = (int)ceil(transfer_time); 
    return rounded_time;
}


void execute_commands(void)
{
    // create first process
    PCB initial_process;
    create_PCB(&initial_process, &commands[0]);


    // initialise queues and structs
    Queue ready_queue;
    ready_queue.front = ready_queue.rear = ready_queue.size = 0;

    Queue sleeping;
    sleeping.front = sleeping.rear = sleeping.size = 0;

    Queue waiting;
    waiting.front = waiting.rear = waiting.size = 0;

    Queue active_children;
    active_children.front = active_children.rear = active_children.size = 0;

    Queue reading;
    reading.front = reading.rear = reading.size = 0;

    Queue writing;
    writing.front = writing.rear = writing.size = 0;

    Queue requesting_io; 
    requesting_io.front = requesting_io.rear = requesting_io.size = 0;

    Queue completed_io;
    completed_io.front = completed_io.rear = completed_io.size = 0;

    DataBus databus;
    databus.state = UNOCCUPIED;

    // Add first PCB to ready queue
    enqueue(&ready_queue, &initial_process);
    initial_process.state = READY;

    PCB *proc = NULL;

    int remaining_tq = time_quantum;

    // Start incrementing total_time by 1usec for every loop
    while (num_processes > 0) {
        
        total_time++;

        // Check if databus can be freed
        if (databus.state == OCCUPIED) {
            if (total_time >= databus.target_time) {
                databus.target_time = -1;
                databus.state = UNOCCUPIED;

                PCB *temp;
                temp = dequeue(&requesting_io);
                enqueue(&completed_io, temp);

                printf("%d+ I/O completed, DATABUS now idle\n", total_time);
            }
        }

        // Find new PCB
        if (proc == NULL) {
            total_time--; 
            
            if (sleeping.size > 0) {
                    proc = find_woke(&sleeping);
                    total_time++;                    
            }
            
            else if (waiting.size > 0 && find_waiting(&waiting, &active_children) != NULL) {
                if (active_children.size == 0) {        //multiple processes waiting whose children have finished?
                    proc = dequeue(&waiting);
                    proc->state = READY;
                    proc->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS;
                    printf("pid%d.WAITING -> READY\n", proc->pid);
                }

                else {
                    proc = find_waiting(&waiting, &active_children);
                    if (proc != NULL) {
                        printf("function worked\n");
                    }
                }
            }

            else if (completed_io.size > 0) {
                proc = dequeue(&completed_io);
                proc->state = READY;
                proc->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS;
                printf("%d+ pid%d.BLOCKED -> READY\n", total_time, proc->pid);
            }

            else if (requesting_io.size > 0 && databus.state == UNOCCUPIED) {
                    databus.target_time = total_time + TIME_ACQUIRE_BUS + requesting_io.queue[requesting_io.front].target_transfer_time;
                    databus.state = OCCUPIED;

                    printf("%d+ device acquiring DATABUS, transfering data till %dusecs\n", total_time, databus.target_time);
                    continue;
            }

            else if (ready_queue.size > 0) {
                    proc = dequeue(&ready_queue);
                    printf("%d+ Next ready process found\n", total_time);
                    proc->state = RUNNING;
                    proc->remaining_transition_time = TIME_CONTEXT_SWITCH;
                    printf("%d+ pid%d.READY -> RUNNING, transition takes 5usecs\n", total_time, proc->pid);
            } else {
                printf("%d+ idle\n", total_time);
                total_time++;
                continue;
            }
        }
    
        
        printf("%d+\n", total_time);   



        // checks whether proc is transitioning, and if it's finished it changes state
        if (proc->remaining_transition_time != -1) {
            if (proc->remaining_transition_time == 0) {
                proc->remaining_transition_time = -1;

                if (proc->state == RUNNING) {
                    printf("%d+ pid%d now on CPU, gets new timequantum\n", total_time, proc->pid);
                }

                else if (proc->state == READY) {
                    enqueue(&ready_queue, proc);
                    printf("%d+ process %d successfully moved to READY\n", total_time, proc->pid);
                    proc = NULL;
                    continue;
                }             
                else if (proc->state == BLOCKED) {
                    enqueue(&sleeping, proc);
                    printf("%d+ process %d successfully moved to SLEEPING\n", total_time, proc->pid);
                    proc = NULL;
                    continue;
                }     
                else if (proc->state == WAITING) {
                    enqueue(&waiting, proc);
                    printf("%d+ process %d successfully moved to WAITING\n", total_time, proc->pid);
                    proc = NULL;
                    continue;
                }
                else if (proc->state == READING || proc->state == WRITING) {
                    enqueue(&requesting_io, proc);
                    total_time--;
                    if (proc->state == READING) {
                    printf("%d+ process %d successfully moved to READING\n", total_time, proc->pid);
                    }
                    else {
                        printf("%d+ process %d successfully moved to WRITING\n", total_time, proc->pid);
                    }
                    proc = NULL;
                    continue;
                }

            } else if (proc->remaining_transition_time > 0) {
                proc->remaining_transition_time--;
                continue;
            }
        }
        
        // Handles timequantum and any currently executing syscalls
        if (proc->state == RUNNING && proc->remaining_transition_time  == -1) {
            
            if (remaining_tq == 0) {
                printf("%d+ process %d timequantum expired!\n", total_time, proc->pid);
                remaining_tq = time_quantum;
                proc->state = READY;
                proc->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS;

                total_time--;
                continue;
            } else {
                remaining_tq--;
            }

            if (proc->remaining_syscall_time != 0) {
                proc->remaining_syscall_time--;
                cpu_time++;
                continue;
            } else {
                char *syscall_type = commands[proc->command_index].syscalls[proc->current_syscall_index].syscall_name;

                if (strcmp(syscall_type, "exit") == 0) {
                    proc->state = EXIT;
                    num_processes--;
                    total_time++;
                    remaining_tq = time_quantum;

                    if (proc->parent_pid != -1) {
                        remove_pcb_from_queue(&active_children, proc);
                    }

                    printf("%d+ syscall exit: process %d exiting\n", total_time, proc->pid);
                    proc = NULL;

                    continue;
                } 
                
                else if (strcmp(syscall_type, "wait") == 0) {

                    total_time++;
                    remaining_tq = time_quantum;
                    
                    if (check_for_children(&active_children, proc->child_pids, proc->num_children) == 1) {
                        proc->state = WAITING;
                        printf("%d+ waiting (children), pid%d RUNNING -> WAITING\n", total_time, proc->pid);
                    } else {
                        proc->state = READY;
                        printf("%d+ waiting (no children), pid%d RUNNING -> READY\n", total_time, proc->pid);
                    }
                    proc->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS;
                    advance_syscall(proc);

                    printf("size of waiting queue -> %d\n", waiting.size);
                }
                
                else if (strcmp(syscall_type, "sleep") == 0) {

                    total_time++;
                    remaining_tq = time_quantum;
                    
                    int sleep_duration = atoi(commands[proc->command_index].syscalls[proc->current_syscall_index].arg3);
                    
                    proc->state = BLOCKED;
                    proc->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS;

                    
                    proc->target_sleep_time = total_time + sleep_duration;
                    advance_syscall(proc);

                    printf("%d+ pid%d.RUNNING -> SLEEPING, transition takes 10usecs\n", total_time, proc->pid);

                }
                
                else if (strcmp(syscall_type, "spawn") == 0) {
                    
                    remaining_tq = time_quantum;
                    
                    char *command_spawned = commands[proc->command_index].syscalls[proc->current_syscall_index].arg3;
                    PCB new_process;
                    for (int i = 0; i < num_commands; i++) {
                        if (strcmp(commands[i].command_name, command_spawned) == 0) {
                            create_PCB(&new_process, &commands[i]);
                            break;
                        }
                    }


                    new_process.state = READY;
                    new_process.remaining_transition_time = -1;
                    new_process.parent_pid = proc->pid;

                    enqueue(&active_children, &new_process);

                    enqueue(&ready_queue, &new_process);
                    printf("this process has just been added to the ready queue -> %d\n",ready_queue.queue[ready_queue.front].pid);

                    printf("%d+ %s now in ready\n", total_time, command_spawned);

                    proc->state = READY;
                    proc->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS;
                    proc->child_pids[proc->num_children] = new_process.pid;
                    proc->num_children++;
                    advance_syscall(proc);

                    printf("%d+ process %d transitioning to ready\n", total_time, proc->pid);

                } 
                
                else if (strcmp(syscall_type, "read") == 0 || strcmp(syscall_type, "write") == 0) {

                    total_time++;

                    int size = atoi(commands[proc->command_index].syscalls[proc->current_syscall_index].arg4);

                    for (int i = 0; i < num_devices; i++) {
                        if (strcmp(commands[proc->command_index].syscalls[proc->current_syscall_index].arg3, devices[i].device_name) == 0) {
                            if (strcmp(syscall_type, "read") == 0) {
                                proc->target_transfer_time = calculate_transfer_time(atoi(devices[i].readspeed), size);
                            }
                            else {
                                proc->target_transfer_time = calculate_transfer_time(atoi(devices[i].writespeed), size);
                            }
                            break;
                        }
                    }
                    if (strcmp(syscall_type, "read") == 0) {
                        proc->state = READING;
                        printf("%d+ pid%d.RUNNING -> READING, transition takes 10usecs\n", total_time, proc->pid);

                    }
                    else {
                        proc->state = WRITING;
                        printf("%d+ pid%d.RUNNING -> WRITING, transition takes 10usecs\n", total_time, proc->pid);

                    }
                    proc->remaining_transition_time = TIME_CORE_STATE_TRANSITIONS; 
                    advance_syscall(proc);
                }
            }
        } 
        printf("%d+\n", total_time);
    }
    printf("%d+ nprocesses = 0\n%d+ command execution complete\n", total_time, total_time);
}

//  ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    if(argc != 3) {
        printf("Usage: %s sysconfig-file command-file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    read_sysconfig(argv[1]);

    read_commands(argv[2]);

    execute_commands();

    int cpu_usage = (int)(((float)cpu_time / total_time) * 100);

    printf("measurements  %i  %i\n", total_time, cpu_usage);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4
