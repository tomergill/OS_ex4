/*******************************************************************************
 * Student name: Tomer Gill
 * Student: 318459450
 * Course Exercise Group: 01 (CS student, actual group is 89231-03)
 * Exercise name: Exercise 4
*******************************************************************************/
#include <stdio.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define FILE_NAME    "./318459450.txt" //file for key
#define KEY_FILE        FILE_NAME      //file for key
#define KEY_CHAR       'T'             //char for key
#define SHM_SIZE        1024           //size of shared mem
#define SEMNUM          3              //num of semaphores
#define SEM_WRITE       0              //index of write semaphore
#define SEM_READ        1              //index of read semaphore
#define LOCK           -1              //the sem_op for lock
#define UNLOCK          1              //the sem_op for unlock
#define THREADPOOL_SIZE 5              //number of threads in threadpool
#define SEM_QUEUE       2              //index of the queue semaphore

/* for semctl */
union semun
{
    int val;
    struct semid_ds *buf;
    ushort *array;
};

/* represents an item in a linked-list queue holding a char */
typedef struct QueueItem_t
{
    char action;
    struct QueueItem_t *next;
} QueueItem;

/* linked-list queue, holding pointers to the first and last nodes in queue */
typedef struct Queue_t
{
    QueueItem *first;
    QueueItem *end;
} Queue;

/****************************** Global Section ********************************/
Queue           *jobQueue                           = NULL;
char            *data                               = NULL;
pthread_t       threadPool[THREADPOOL_SIZE];
int             semid                               = -1;
pthread_mutex_t queueMutex, countMutex, fileMutex;
int             internal_count                      =  0;
int             shmid                               = -1;
/******************************************************************************/

/******************************************************************************
 * function name: Dequeue
 * The Input: None.
 * The output: Gets the first char (job) in the jobQueue and pulling it out
 *      of the queue and returning it. If the queue is empty waits to it to
 *      fill.
 * The Function operation: Decreases the queue semaphore (aka saying there is
 *      one less item in the queue now - if there were 0 the thread waits to
 *      be awaken when the semaphore gets its value up). Also, the queue
 *      mutex is locked so no other thread will try to use it until the job
 *      is out. Then the top job is dequeued, the mutex unlocked and the
 *      former-top job is returned.
*******************************************************************************/
char Dequeue()
{
    char job;

    /* decreasing the queue semaphore */
    struct sembuf sops[1];
    sops->sem_flg = 0;
    sops->sem_num = SEM_QUEUE;
    sops->sem_op = -1; //"lock"
    if (semop(semid, sops, 1) != 0)
        perror("-1 queue semaphore error dequeue");

    /* locking the queue mutex */
    if (pthread_mutex_lock(&queueMutex) != 0)
    {
        perror("error locking queueMutex in dequeue");
        sops->sem_num = SEM_QUEUE;
        sops->sem_op = 1; //"unlock"
        if (semop(semid, sops, 1) != 0)
            perror("+1 queue semaphore error dequeue");
        return (char) -1;
    }

    if (jobQueue->first == NULL) //no elements in queue - shouldn't happen
        job = (char) -1;
    else
    {
        job = jobQueue->first->action;

        if (jobQueue->first == jobQueue->end) //only one item in queue
            jobQueue->end = NULL;

        QueueItem *temp = jobQueue->first;
        jobQueue->first = jobQueue->first->next; //"pop"
        temp->next = NULL;
        free(temp); //free item
    }

    if (pthread_mutex_unlock(&queueMutex) != 0)
    {
        perror("error unlocking queueMutex in dequeue");
        return (char) -1;
    }


    return job;
}

/******************************************************************************
 * function name: Enqueue
 * The Input: A char (job) to insert to the queue.
 * The output: Adds the job to the end of the queue.
 * The Function operation: A new queue item is created, then gets the right
 *      data. The queue mutex is locked so no other thread will try to use it
 *      until the job is in. Then the new item is added to the end of the
 *      queue, and the function increases the queue semaphore (aka saying
 *      there is one more item in the queue now - if there were 0 the threads
 *      waiting to be awaken could continue their operations), and the mutex
 *      unlocked.
*******************************************************************************/
void Enqueue(char c)
{

    QueueItem *item = (QueueItem *) calloc(1, sizeof(QueueItem));
    if (item == NULL)
    {
        perror("CALLOC ERROR");
        return;
    }
    item->next = NULL;
    item->action = c;

    if (pthread_mutex_lock(&queueMutex) != 0)
    {
        perror("error locking queueMutex in enqueue");
        return;
    }
    if (jobQueue->end == NULL) //nothing in queue
        jobQueue->first = jobQueue->end = item;
    else
        jobQueue->end = jobQueue->end->next = item;

    struct sembuf sops[1];
    sops->sem_flg = 0;
    sops->sem_num = SEM_QUEUE;
    sops->sem_op = 1; //"unlock"
    if (semop(semid, sops, 1) != 0)
        perror("+1 queue semaphore error enqueue");

    if (pthread_mutex_unlock(&queueMutex) != 0)
    {
        perror("error unlocking queueMutex in enqueue");
        return;
    }
}

/******************************************************************************
 * function name: AtExitFunc
 * The Input: None.
 * The output: Cleans the resources the process uses and then continues the
 *      exit. Happens after exit() is called.
 * The Function operation: Frees each item in the jobQueue, and then the
 *      queue itself. Then detaches from the shared memory, deletes it,
 *      the semaphores and the mutexes (which is unlocked in case another thread
 *      locked them and died before it could unlock them),
*******************************************************************************/
void AtExitFunc()
{
    /* freeing the queue */
    if (jobQueue != NULL)
    {
        QueueItem *i = jobQueue->first, *temp;
        while (i != NULL)
        {
            temp = i;
            i = i->next;
            free(temp);
        }
        free(jobQueue);
    }

    if (data != NULL)
        if (shmdt(data) == -1)
            perror("shared memory detach error");
    if (shmid >= 0)
        if (shmctl(shmid, IPC_RMID, NULL) == -1)
            perror("error deleting shared mem");
    if (semid >= 0)
        if (semctl(semid, SEMNUM, IPC_RMID) == -1)
            perror("delete semaphores failed");

    //unlocking in case mutex was locked when exiting
    pthread_mutex_unlock(&queueMutex);
    pthread_mutex_unlock(&countMutex);
    pthread_mutex_unlock(&fileMutex);

    if (pthread_mutex_destroy(&queueMutex) != 0)
        perror("error destroying queue mutex");
    if (pthread_mutex_destroy(&countMutex) != 0)
        perror("error destroying count mutex");
    if (pthread_mutex_destroy(&fileMutex) != 0)
        perror("error destroying file mutex");
}

/******************************************************************************
 * function name: AddToInternalCount
 * The Input: Amount to add to internal count.
 * The output: Adds the requested amount to internal count.
 * The Function operation: Locks the internal count mutex, adds the amount
 *      and then unlocks said mutex.
*******************************************************************************/
void AddToInternalCount(int amount)
{
    if (pthread_mutex_lock(&countMutex) != 0)
    {
        perror("error locking countMutex in add");
        return;
    }
    internal_count += amount;
    if (pthread_mutex_unlock(&countMutex) != 0)
    {
        perror("error unlocking countMutex in add");
        return;
    }
}

/******************************************************************************
 * function name: GetInternalCount
 * The Input: None.
 * The output: Gets the current value of internal count.
 * The Function operation: Locks the internal count mutex, saves the value
 *      and then unlocks said mutex and reutrns the saved value.
*******************************************************************************/
int GetInternalCount()
{
    int temp;
    if (pthread_mutex_lock(&countMutex) != 0)
    {
        perror("error locking countMutex in add");
        return -1;
    }
    temp = internal_count;
    if (pthread_mutex_unlock(&countMutex) != 0)
    {
        perror("error unlocking countMutex in add");
        return -1;
    }
    return temp;
}

/******************************************************************************
 * function name: WriteToFileInternalCount
 * The Input: File descriptor to write to.
 * The output: Writes the thread id and internal count to the file.
 * The Function operation: Locks the file mutex, writes and then unlocks.
*******************************************************************************/
void WriteToFileInternalCount(int fd)
{
    int temp = GetInternalCount();
    char buff[100];
    sprintf(buff, "thread identifier is %lu and internal_count is %d\n",
            pthread_self(), temp);
    if (pthread_mutex_lock(&fileMutex) != 0)
    {
        perror("error locking fileMutex in write");
        exit(EXIT_FAILURE);
    }
    if (write(fd, buff, strlen(buff)) == -1)
    {
        perror("write error");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_unlock(&fileMutex) != 0)
    {
        perror("error unlocking fileMutex in write");
        exit(EXIT_FAILURE);
    }
}

/******************************************************************************
 * function name: ThreadFunc
 * The Input: File descriptor to write to.
 * The output: Does jobs from the job queue until gets a job to terminate.
 * The Function operation: Gets the first job form the job queue, does it
 *      (adds to internal count / writes to the file / terminates) on and on.
*******************************************************************************/
void * ThreadFunc(void *arg)
{
    int fd = *(int *) arg, x, amount;
    char action;
    struct timespec t;

    while (1)
    {
        amount = 0;
        action = Dequeue(); //get the job
        if (action == (char) -1)
            continue;
        switch (action)
        {
            default:
                break;
            case 'e':
                ++amount; //+5
            case 'd':
                ++amount; //+4
            case 'c':
                ++amount; //+3
            case 'b':
                ++amount; //+2
            case 'a':
                ++amount; //+1
                break;
            case 'f': //cancel all
                WriteToFileInternalCount(fd);
                continue;
            case 'X': //exit thread
                WriteToFileInternalCount(fd);
                pthread_exit(NULL);
        }

        // for letters a to e
        x = rand() % 91 + 10; // 10<=x<=100
        t.tv_nsec = x;
        t.tv_sec = 0;
        if (nanosleep(&t, NULL) != 0)
            perror("nanosleep error");
        AddToInternalCount(amount);
    }
}

/******************************************************************************
 * function name: main
 * The Input: None.
 * The output: Creates a thread pool to handle jobs from the clients and
 *      continues to bring jobs from the shared memory (written to by
 *      clients) to the job queue.
 * The Function operation: Creates a 318459450.txt file, the job queue, the
 *      shared memory, the semaphores and the mutexes. Then in a loop
 *      decreases the read semaphore (saying it now reads - if it is 0 the
 *      server is stuck because no client has written anything), reads the
 *      job, increases the write semaphore (saying it's ready to get another
 *      job) and then adds the job read to the queue.
 *      This is done until got 'h' - adds termination jobs to all threads
 *      after all their jobs and waits for them - or 'g' - cancels all the
 *      threads, then writes to the file and exits.
*******************************************************************************/
int main()
{
    int fd, i;
    key_t shmKey, semKey;
    struct sembuf sops[SEMNUM];
    union semun semarg;
    pthread_mutexattr_t Attr;

    if ((fd = open(FILE_NAME, O_CREAT | O_RDWR | O_TRUNC, 0666)) < 0)
    {
        perror("open error");
        exit(EXIT_FAILURE);
    }

    jobQueue = (Queue *) malloc(sizeof(struct Queue_t));
    if (jobQueue == NULL)
    {
        perror("malloc error");
        exit(EXIT_FAILURE);
    }
    if (atexit(AtExitFunc) == -1)
        perror("atexit error");

    /* get shmKey to shared memory */
    if ((shmKey = ftok(FILE_NAME, KEY_CHAR)) == -1)
    {
        perror("ftok error");
        exit(EXIT_FAILURE);
    }

    /* create a shared memory */
    if ((shmid = shmget(shmKey, SHM_SIZE, 0666 | IPC_CREAT)) == -1)
    {
        perror("server shmget error");
        exit(EXIT_FAILURE);
    }

    /* attach to the segment to get a pointer to it: */
    data = shmat(shmid, NULL, 0);
    if (data == (char *) (-1))
    {
        perror("shmat error");
        exit(EXIT_FAILURE);
    }

    /* get semKey to semaphores */
    if ((semKey = ftok(KEY_FILE, KEY_CHAR + 1)) == -1)
    {
        perror("ftok error 2");
        exit(EXIT_FAILURE);
    }

    /*
     * Creating the read, write and queue semaphores:
     * Read - 0 means the server just read a char from the shared mem, and no
     *      client has written. 1 means a client wrote and the server can read.
     * Write - 0 means the server isn;t ready to receive another char from
     *      client. 1 means it read and waits for another one.
     * Queue - the value represents the number of items in the job queue. if
     *      a thread wants to get an item from the queue (dequeue) it
     *      decreases the semaphore and if it reaches a negative value the
     *      thread waits until the main thread adds more jobs, thus
     *      increasing the value (enqueue).
     */
    if ((semid = semget(semKey, SEMNUM, 0666 | IPC_CREAT)) < 0)
    {
        perror("semget error");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutexattr_init(&Attr) != 0)
    {
        perror("attribute init failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_ERRORCHECK) != 0)
    {
        perror("attribute settype failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&queueMutex, &Attr) != 0)
    {
        perror("queue mutex error");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&countMutex, &Attr) != 0)
    {
        perror("count mutex error");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&fileMutex, &Attr) != 0)
    {
        perror("file mutex error");
        exit(EXIT_FAILURE);
    }

    semarg.array = (unsigned short *) malloc(SEMNUM);
    if (semarg.array == NULL)
    {
        perror("malloc error 1");
        exit(EXIT_FAILURE);
    }
    //setting values
    semarg.array[SEM_READ] = 0;
    semarg.array[SEM_WRITE] = 1;
    semarg.array[SEM_QUEUE] = 0;
    if (semctl(semid, SEMNUM, SETALL, semarg) == -1)
    {
        perror("SETALL error");
        exit(EXIT_FAILURE);
    }
    free(semarg.array);

    for (i = 0; i < THREADPOOL_SIZE; ++i)
    {
        if (pthread_create(&(threadPool[i]), NULL, ThreadFunc, &fd) == -1)
        {
            perror("error creating thread");
            exit(EXIT_FAILURE);
        }
    }

    sops[0].sem_flg = 0;
    /* start reading the shared memory until finished by another thread */
    while (1)
    {
        /* locking the read semaphore */
        sops[0].sem_op = LOCK;
        sops[0].sem_num = SEM_READ;
        if (semop(semid, sops, 1) == -1)
        {
            perror("error locking SEMREAD");
            continue;
        }

        /* reading the char and inserting it to the buffer */
        char temp = *data;
        *data = '\0';

        /* unlocking the write semaphore */
        sops[0].sem_op = UNLOCK;
        sops[0].sem_num = SEM_WRITE;
        if (semop(semid, sops, 1) == -1)
        {
            perror("error unlocking SEMWRITE");
            continue;
        }

        switch (temp)
        {
            case 'g':
                for (i = 0; i < THREADPOOL_SIZE; ++i)
                    if (pthread_cancel(threadPool[i]) != 0)
                        perror("error closing thread");
                WriteToFileInternalCount(fd);
                if (close(fd) != 0)
                    perror("close error g");
                exit(EXIT_SUCCESS);

            case 'h': //join all
                for (i = 0; i < THREADPOOL_SIZE; ++i)
                    Enqueue('X');
                for (i = 0; i < THREADPOOL_SIZE; ++i)
                    if (pthread_join(threadPool[i], NULL) != 0)
                        perror("error joining thread");
                WriteToFileInternalCount(fd);
                if (close(fd) != 0)
                    perror("close error h");
                exit(EXIT_SUCCESS);
            default:
                Enqueue(temp);
                break;
        }
    }
}