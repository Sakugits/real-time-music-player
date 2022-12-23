/**********************************************************
 *  INCLUDES
 *********************************************************/
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <rtems.h>
#include <rtems/termiostypes.h>
#include <rtems/shell.h>
#include <rtems/untar.h>
#include <bsp.h>

/**********************************************************
 *  CONSTANTS
 *********************************************************/
#define NSEC_PER_SEC 1000000000UL //nanoseconds in a second

#define DEV_NAME "/dev/com1" //Arduino comms by default
#define FILE_NAME "/let_it_be.raw"

#define PERIOD_TASK_DISPLAY_STATUS_SEC   5     /* Seconds of Task DISPLAY STATUS Period*/
#define PERIOD_TASK_DISPLAY_STATUS_NSEC   0    /* Nano Seconds of Task DISPLAY STATUS Period*/

#define PERIOD_TASK_RECEIVE_STATUS_SEC   2      /* Seconds of Task DISPLAY STATUS Period*/
#define PERIOD_TASK_RECEIVE_STATUS_NSEC   0     /* Nano Seconds of Task DISPLAY STATUS Period*/

#define PERIOD_TASK_READ_SEND_SEC  0                /* Seconds of Task SEND Period*/
#define PERIOD_TASK_READ_SEND_NSEC  64000000       /* Nano Seconds of Task SEND Period*/

#define SEND_SIZE 256                          /* BYTES */
#define BYTE_SIZE 8

#define TARFILE_START _binary_tarfile_start
#define TARFILE_SIZE _binary_tarfile_size

#define SLAVE_ADDR 0x8

/**********************************************************
 *  GLOBALS
 *********************************************************/
extern int _binary_tarfile_start;
extern int _binary_tarfile_size;

// Variable to store te current state (Reproducing or paused)
int pauseReproductionState = 0;

// Priorities for each task
struct sched_param displayStatusPriority = {
    sched_priority:1
};
struct sched_param receiveStatusPriority = {
    sched_priority:2
};
struct sched_param readSendPriority = {
    sched_priority:3
};

// Threads Definition
pthread_t  readSendThread, displayStatusThread, receiveStatusThread;

// Attributes for each thread
pthread_attr_t readSendAttributes, displayStatusAttributes, receiveStatusAttributes;

/**********************************************************
 *  MUTEX
 *********************************************************/

pthread_mutex_t mutex;
pthread_mutexattr_t mutexattributes;

/**********************************************************
 * Function: diffTime
 *********************************************************/
void diffTime(struct timespec end,
              struct timespec start,
              struct timespec *diff)
{
    if (end.tv_nsec < start.tv_nsec) {
        diff->tv_nsec = NSEC_PER_SEC - start.tv_nsec + end.tv_nsec;
        diff->tv_sec = end.tv_sec - (start.tv_sec+1);
    } else {
        diff->tv_nsec = end.tv_nsec - start.tv_nsec;
        diff->tv_sec = end.tv_sec - start.tv_sec;
    }
}

/**********************************************************
 * Function: addTime
 *********************************************************/
void addTime(struct timespec end,
              struct timespec start,
              struct timespec *add)
{
    unsigned long aux;
    aux = start.tv_nsec + end.tv_nsec;
    add->tv_sec = start.tv_sec + end.tv_sec +
                  (aux / NSEC_PER_SEC);
    add->tv_nsec = aux % NSEC_PER_SEC;
}

/**********************************************************
 * Function: compTime
 *********************************************************/
int compTime(struct timespec t1,
              struct timespec t2)
{
    if (t1.tv_sec == t2.tv_sec) {
        if (t1.tv_nsec == t2.tv_nsec) {
            return (0);
        } else if (t1.tv_nsec > t2.tv_nsec) {
            return (1);
        } else if (t1.tv_sec < t2.tv_sec) {
            return (-1);
        }
    } else if (t1.tv_sec > t2.tv_sec) {
        return (1);
    } else if (t1.tv_sec < t2.tv_sec) {
        return (-1);
    }
    return (0);
}

/**********************************************************
 *  Function: display_status
 *********************************************************/
void * display_status(void *param) {

    // Variables for time
    struct timespec start,end,diff,cycle;

    // Copy values of cycle duration
    cycle.tv_sec=PERIOD_TASK_DISPLAY_STATUS_SEC;
    cycle.tv_nsec=PERIOD_TASK_DISPLAY_STATUS_NSEC;

    // Get start time
    clock_gettime(CLOCK_REALTIME,&start);

    while (1) {

        pthread_mutex_lock(&mutex);

        if(pauseReproductionState == 1){

            pthread_mutex_unlock(&mutex);
            printf("Music Stopped...\n");

        } 
        else{

            pthread_mutex_unlock(&mutex);
            printf("Playing Music...\n");

        }

        // Get end time, calculate lapso and sleep
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);

        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: time limit surpassed in display_status\n");
            exit(-1);
        }

        diffTime(cycle,diff,&diff);
        nanosleep(&diff,NULL);
        addTime(start,cycle,&start);
    }
}

/**********************************************************
 *  Function: receive_status
 *********************************************************/
void * receive_status(void *param) {

    //Aux variable for input control
    char keyboardInput = '1';

    // Variables for time
    struct timespec start,end,diff,cycle;

    // Copy values of cycle duraation
    cycle.tv_sec=PERIOD_TASK_RECEIVE_STATUS_SEC;
    cycle.tv_nsec=PERIOD_TASK_RECEIVE_STATUS_NSEC;

    // Get start time
    clock_gettime(CLOCK_REALTIME,&start);

    while (1) {

        // Check input value for '0' pause, for '1' resume.

        if(keyboardInput == '1'){

            pthread_mutex_lock(&mutex);
            pauseReproductionState = 0;
            pthread_mutex_unlock(&mutex);

        } 
        else if(keyboardInput == '0'){

            pthread_mutex_lock(&mutex);
            pauseReproductionState = 1;
            pthread_mutex_unlock(&mutex);

        }

        // Get state from input
        while(0 >= scanf("%c", &keyboardInput));
        // Get end time, calculate lapso and sleep
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        diff.tv_sec = diff.tv_sec % PERIOD_TASK_RECEIVE_STATUS_SEC;

        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: time limit surpassed in receive_status\n");
            exit(-1);
        }

        diffTime(cycle,diff,&diff);
        nanosleep(&diff,NULL);
        addTime(start,cycle,&start);
    }    
}
/**********************************************************
 *  Function: read_send
 *********************************************************/
void * read_send(void *param) {

    // Variables for time
    struct timespec start,end,diff,cycle;

    unsigned char buf[SEND_SIZE];
    int fd_file = -1;
    int fd_serie = -1;
    int ret = 0;


    /* Open serial port */
    printf("open serial device %s \n",DEV_NAME);
    fd_serie = open (DEV_NAME, O_RDWR);

    if (fd_serie < 0) {
        printf("ERROR in open: error opening serial %s\n", DEV_NAME);
        exit(-1);
    }

    struct termios portSettings;
    speed_t speed=B115200;

    tcgetattr(fd_serie, &portSettings);
    cfsetispeed(&portSettings, speed);
    cfsetospeed(&portSettings, speed);
    cfmakeraw(&portSettings);
    tcsetattr(fd_serie, TCSANOW, &portSettings);

    /* Open music file */
    printf("open file %s begin\n",FILE_NAME);
    fd_file = open (FILE_NAME, O_RDWR);
    if (fd_file < 0) {
        perror("ERROR in open: error opening file \n");
        exit(-1);
    }

    // Copy values of cycle duration
    cycle.tv_sec=PERIOD_TASK_READ_SEND_SEC;
    cycle.tv_nsec=PERIOD_TASK_READ_SEND_NSEC;

    // Get start time
    clock_gettime(CLOCK_REALTIME,&start);

    while (1) {

        pthread_mutex_lock(&mutex);

        if (pauseReproductionState== 1) { //Music stopped

            pthread_mutex_unlock(&mutex);
            memset(buf, 0, SEND_SIZE);
            ret=write(fd_serie,buf,SEND_SIZE);

        } 
        
        else { //Music playing

            pthread_mutex_unlock(&mutex);

            for (int i=0; i<=SEND_SIZE/BYTE_SIZE; i++) { //es necesario mandarlos de 1 en 1
				// Read from music file
					ret=read(fd_file,buf,BYTE_SIZE); //cargar musica

				if (ret < 0) {
					printf("ERROR in read: error reading file\n");
					exit(-1);
				}

				// Write on the serial/I2C port
				ret=write(fd_serie,buf,BYTE_SIZE); //enviarla por serial

            }
        }

        // Checking if any error while writing
        if (ret < 0) {
            printf("ERROR in write: error writing serial\n");
            exit(-1);
        }

        // Get end time, calculate lapso and sleep
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: time limit surpassed in read_send\n");
            exit(-1);
        }
        diffTime(cycle,diff,&diff);
        nanosleep(&diff,NULL);
        addTime(start,cycle,&start);
    }    
}

/*****************************************************************************
 * Function: Init()
 *****************************************************************************/
rtems_task Init (rtems_task_argument ignored)
{

    printf("Populating Root file system from TAR file.\n");
    Untar_FromMemory((unsigned char *)(&TARFILE_START),
                     (unsigned long)&TARFILE_SIZE);

    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4,
                     100, "/dev/foobar", false, true, NULL);

    // Mutex configuration for syncronizing pauseReproductionState
    pthread_mutexattr_init(&mutexattributes);
    pthread_mutexattr_setprotocol(&mutexattributes, PTHREAD_PRIO_PROTECT);
    pthread_mutexattr_setprioceiling(&mutexattributes, 3);
    pthread_mutex_init(&mutex, &mutexattributes);

    //THREAD 1 (for task read_send)
    pthread_attr_init(&readSendAttributes);
    pthread_attr_setinheritsched(&readSendAttributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&readSendAttributes, SCHED_FIFO);
    pthread_attr_setschedparam(&readSendAttributes, &readSendPriority);
    pthread_create(&readSendThread, &readSendAttributes, &read_send, NULL);

    //THREAD 2 (for task receive_status)
    pthread_attr_init(&receiveStatusAttributes);
    pthread_attr_setinheritsched(&receiveStatusAttributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&receiveStatusAttributes, SCHED_FIFO);
    pthread_attr_setschedparam(&receiveStatusAttributes, &receiveStatusPriority);
    pthread_create(&receiveStatusThread, &receiveStatusAttributes, &receive_status, NULL);

    //THREAD 3 (for task display_status)
    pthread_attr_init(&displayStatusAttributes);
    pthread_attr_setinheritsched(&displayStatusAttributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&displayStatusAttributes, SCHED_FIFO);
    pthread_attr_setschedparam(&displayStatusAttributes, &displayStatusPriority);
    pthread_create(&displayStatusThread, &displayStatusAttributes, &display_status,  NULL);


    // Ending and destroying the threads and attributes
    pthread_join(displayStatusThread, NULL);
    pthread_join(receiveStatusThread, NULL);
    pthread_join(readSendThread, NULL);
    
    pthread_attr_destroy(&displayStatusAttributes);
    pthread_attr_destroy(&receiveStatusAttributes);
    pthread_attr_destroy(&displayStatusAttributes);

    // Destroying the mutex
    pthread_mutex_destroy(&mutex);

    exit(0);

} /* End of Init() */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 20
#define CONFIGURE_UNIFIED_WORK_AREAS
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_INIT
#include <rtems/confdefs.h>

