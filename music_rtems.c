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
#define NSEC_PER_SEC 1000000000UL

#define DEV_NAME "/dev/com1"
#define FILE_NAME "/let_it_be_1bit.raw"

#define PERIOD_TASK_DISPLAY_STATUS_SEC   5     /* Secoonds of Task DISPLAY STATUS Period*/
#define PERIOD_TASK_DISPLAY_STATUS_NSEC   0    /* Nano Seconds of Task DISPLAY STATUS Period*/

#define PERIOD_TASK_RECEIVE_STATUS_SEC   2      /* Secoonds of Task DISPLAY STATUS Period*/
#define PERIOD_TASK_RECEIVE_STATUS_NSEC   0     /* Nano Seconds of Task DISPLAY STATUS Period*/

#define PERIOD_TASK_READ_SEND_SEC  0                /* Seconds of Task SEND Period*/
#define PERIOD_TASK_READ_SEND_NSEC  512000000       /* Nano Seconds of Task SEND Period*/

#define SEND_SIZE 256                          /* BYTES */

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
pthread_t  readSend, displayStatus, receiveStatus;

// Attributes for each thread
pthread_attr_t readSendAttr, displayStatusAttr, receiveStatusAttr;

/**********************************************************
 *  MUTEX
 *********************************************************/

pthread_mutex_t mutex;
pthread_mutexattr_t mutexattr;

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

    // Copy values of cycle duraation
    cycle.tv_sec=PERIOD_TASK_DISPLAY_STATUS_SEC;
    cycle.tv_nsec=PERIOD_TASK_DISPLAY_STATUS_NSEC;

    // Getstart time
    clock_gettime(CLOCK_REALTIME,&start);

    while (1) {

        pthread_mutex_lock(&mutex);

        if(pauseReproductionState == 1){

            pthread_mutex_unlock(&mutex);
            printf("Reproduction paused\n");

        } 
        else{

            pthread_mutex_unlock(&mutex);
            printf("Reproduction resumed\n");

        }

        // Get end time, calculate lapso and sleep
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: lasted long than the cycle\n");
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
    char keyboardInput = '1'

    // Variables for time
    struct timespec start,end,diff,cycle;

    // Copy values of cycle duraation
    cycle.tv_sec=PERIOD_TASK_RECEIVE_STATUS_SEC;
    cycle.tv_nsec=PERIOD_TASK_RECEIVE_STATUS_NSEC;

    // Getstart time
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
        else{
            printf("ERROR: INPUT ERROR, INCORRECT INPUT\n");
        }

        // Get state from input
        while(0 >= scanf("%c", &keyboardInput));

        // Get end time, calculate lapso and sleep
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: lasted long than the cycle\n");
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

    printf("Populating Root file system from TAR file.\n");
    Untar_FromMemory((unsigned char *)(&TARFILE_START),
                     (unsigned long)&TARFILE_SIZE);

    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4,
                     100, "/dev/foobar", false, true, NULL);

    /* Open serial port */
    printf("open serial device %s \n",DEV_NAME);
    fd_serie = open (DEV_NAME, O_RDWR);
    if (fd_serie < 0) {
        printf("open: error opening serial %s\n", DEV_NAME);
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
        perror("open: error opening file \n");
        exit(-1);
    }

    // Copy values of cycle duraation
    cycle.tv_sec=PERIOD_TASK_READ_SEND_SEC
    cycle.tv_nsec=PERIOD_TASK_READ_SEND_NSEC;

    // Getstart time
    clock_gettime(CLOCK_REALTIME,&start);

    while (1) {

        pthread_mutex_lock(&mutex);

        if (pauseReproductionState== 1){

            pthread_mutex_unlock(&mutex);
            memset(buf, 0, SEND_SIZE);
            ret=write(fd_serie,buf,SEND_SIZE);

        } else {

            pthread_mutex_unlock(&mutex);
            // Read from music file
            ret=read(fd_file,buf,SEND_SIZE);
            if (ret < 0) {
                printf("read: error reading file\n");
                exit(-1);
            }

            // Write on the serial/I2C port
            ret=write(fd_serie,buf,SEND_SIZE);

        }

        // Checking if any error while writing
        if (ret < 0) {
            printf("write: error writing serial\n");
            exit(-1);
        }

        // Get end time, calculate lapso and sleep
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: lasted long than the cycle\n");
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
    struct timespec start,end,diff,cycle;
    unsigned char buf[SEND_SIZE];
    int fd_file = -1;
    int fd_serie = -1;
    int ret = 0;

    printf("Populating Root file system from TAR file.\n");
    Untar_FromMemory((unsigned char *)(&TARFILE_START),
                     (unsigned long)&TARFILE_SIZE);

    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4,
                     100, "/dev/foobar", false, true, NULL);

     // Mutex configuration
   pthread_mutexattr_init(&mutexattr);
   pthread_mutexattr_setprotocol(&mutexattr, PTHREAD_PRIO_PROTECT);
   pthread_mutexattr_setprioceiling(&mutexattr, 3);
   pthread_mutex_init(&mutex, &mutexattr);

	//THREAD 1 (for task display_status)
	pthread_attr_init(&displayStatusAttr);
	pthread_attr_setinheritsched(&displayStatusAttr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&displayStatusAttr, SCHED_FIFO);
	pthread_attr_setschedparam(&displayStatusAttr, &displayStatusPriority);
	pthread_create(&display_status, &displayStatusAttr, displayStatus, NULL);
	

    //THREAD 2 (for task receive_status)
    pthread_attr_init(&receiveStatusAttr);
	pthread_attr_setinheritsched(&receiveStatusAttr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&receiveStatusAttr, SCHED_FIFO);
	pthread_attr_setschedparam(&receiveStatusAttr, &receiveStatusPriority);
	pthread_create(&receive_status, &receiveStatusAttr, receiveStatus, NULL);

    //THREAD 3 (for task read_send)
    pthread_attr_init(&readSendAttr);
	pthread_attr_setinheritsched(&readSendAttr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&readSendAttr, SCHED_FIFO);
	pthread_attr_setschedparam(&readSendAttr, &readSendPriority);
	pthread_create(&read_send, &readSendAttr, readSend, NULL);

    // Ending and destroying the threads and attributes
    pthread_join(displayStatus, NULL);
    pthread_join(receiveStatus, NULL);
    pthread_join(readSend, NULL);
    
    pthread_attr_destroy(&displayStatusAttr);
    pthread_attr_destroy(&receiveStatusAttr);
    pthread_attr_destroy(&displayStatusAttr);

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

