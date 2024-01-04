/*
 * student.c
 * Multithreaded OS Simulation for CS 2200 and ECE 3058
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os-sim.h"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;

// linked list
static pcb_t *head;
static pthread_mutex_t list_mutex;
static pthread_cond_t non_empty_list;

static int scheduling;
static int timeslice;

/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *	The current array (see above) is how you access the currently running process indexed by the cpu id. 
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void list_add(pcb_t* pcb) {
    pcb_t* curr;
    pthread_mutex_lock(&list_mutex);
    if (head==NULL) {
        head = pcb;
    }
    else {
        curr = head;
        while(curr->next!=NULL) {
            curr = curr->next;
        }
        curr->next = pcb;
    }
    pcb->next = NULL;
    pthread_cond_broadcast(&non_empty_list);
    pthread_mutex_unlock(&list_mutex);
}
static pcb_t* list_pop() {
    pcb_t* ret;
    pthread_mutex_lock(&list_mutex);
    ret = head;
    if(ret!=NULL) {
        head = ret->next;
    }
    pthread_mutex_unlock(&list_mutex);
    return ret;
}

static void schedule(unsigned int cpu_id)
{
    pcb_t  *runnable_process;
    pthread_mutex_lock(&current_mutex);
    if(head!=NULL) { //if something in list
        //get head of list
        runnable_process = list_pop();
        runnable_process->state = PROCESS_RUNNING;
        //context switches
        if(scheduling == SCHED_FIFO){
            context_switch(cpu_id, runnable_process, -1);
        } else {
            context_switch(cpu_id, runnable_process, timeslice);
        }
        current[cpu_id] = runnable_process;
    } 
    else {
        if(scheduling == SCHED_FIFO){
            context_switch(cpu_id, NULL, -1);
        } else {
            context_switch(cpu_id, NULL, timeslice);
        }
    }
    pthread_mutex_unlock(&current_mutex);
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
    /* FIX ME */
    pthread_mutex_lock(&list_mutex);
    while(head==NULL) {
        pthread_cond_wait(&non_empty_list, &list_mutex);
    }

    pthread_mutex_unlock(&list_mutex);
    schedule(cpu_id);
    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
    //mt_safe_usleep(1000000);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{
    pcb_t* pcb;
    pthread_mutex_lock(&current_mutex);
    pcb = current[cpu_id];
    pcb->state = PROCESS_READY;
    pthread_mutex_unlock(&current_mutex);
    list_add(pcb);
    schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    pcb_t* pcb;
    pthread_mutex_lock(&current_mutex);
    pcb = current[cpu_id];
    pcb->state = PROCESS_WAITING;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
    pcb_t* pcb;
    pthread_mutex_lock(&current_mutex);
    pcb = current[cpu_id];
    pcb->state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is LRTF, wake_up() may need
 *      to preempt the CPU with lower remaining time left to allow it to
 *      execute the process which just woke up with higher reimaing time.
 * 	However, if any CPU is currently running idle,
* 	or all of the CPUs are running processes
 *      with a higher remaining time left than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
    process->state = PROCESS_READY;
    list_add(process);

}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -l and -r command-line parameters.
 */
int main(int argc, char *argv[])
{
    unsigned int cpu_count;

    /* Parse command-line arguments */
    if (argc < 2 || argc > 4)
    {
        fprintf(stderr, "Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -l | -r <time slice> ]\n"
            "    Default : FIFO Scheduler\n"
	    "         -l : Longest Remaining Time First Scheduler\n"
            "         -r : Round-Robin Scheduler\n\n");
        return -1;
    }
    timeslice = -1;
    scheduling = SCHED_FIFO; //default FIFO
    cpu_count = strtoul(argv[1], NULL, 0);
    
    if(argc>2 && strcmp(argv[2],"-r") == 0) {
        timeslice = atoi(argv[3]);
        scheduling = SCHED_RR;
    }

    /* FIX ME - Add support for -l and -r parameters*/

    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}


