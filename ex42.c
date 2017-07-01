/*******************************************************************************
 * Student name: Tomer Gill
 * Student: 318459450
 * Course Exercise Group: 01 (CS student, actual group is 89231-03)
 * Exercise name: Exercise 4
*******************************************************************************/
#include <stdio.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define FILE_NAME "318459450.txt" //file for key
#define KEY_FILE FILE_NAME        //file for key
#define KEY_CHAR       'T'        //char for key
#define SHM_SIZE     1024         //size of shared mem
#define SEMNUM          2         //num of semaphores
#define SEM_WRITE       0         //index of write semaphore
#define SEM_READ        1         //index of read semaphore
#define LOCK           -1         //the sem_op for lock
#define UNLOCK          1         //the sem_op for unlock
#define THREADPOOL_SIZE 5

union semun {
    int val;
    struct semid_ds *buf;
    ushort *array;
};

typedef struct QueueItem_t
{
    char                action;
    struct QueueItem_t  *next;
} QueueItem;

typedef struct
{
    QueueItem *first;
    QueueItem *end;
} Queue;

/***** Global Section *****/
Queue *jobQueue = NULL;
char *data = NULL;
pthread_t threadPool[THREADPOOL_SIZE];
int semid = -1;
pthread_mutex_t queueMutex, countMutex, fileMutex;
int internal_count = 0;

char Dequeue()
{
    if (pthread_mutex_lock(&queueMutex) != 0)
    {
        perror("error locking queueMutex in dequeue");
        return (char)-1;
    }
    if (jobQueue->first == NULL)
        return (char) -1;

    char c = jobQueue->first->action;

    if (jobQueue->first == jobQueue->end) //only one item in queue
        jobQueue->end = NULL;

    QueueItem *temp = jobQueue->first;
    jobQueue->first = jobQueue->first->next; //"pop"

    if (pthread_mutex_unlock(&queueMutex) != 0)
    {
        perror("error unlocking queueMutex in dequeue");
        return (char)-1;
    }

    temp->next = NULL;
    free(temp); //free item

    return c;
}

void Enqueue(char c)
{
    QueueItem *item = (QueueItem *)calloc(1, sizeof(QueueItem));
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
    if (pthread_mutex_unlock(&queueMutex) != 0)
    {
        perror("error unlocking queueMutex in enqueue");
        return;
    }
}

void atExitFunc() {
    if (jobQueue != NULL)
        free(jobQueue);
    if (data != NULL)
        if (shmdt(data) == -1)
            perror("shared memory detach error");
    if (semid >= 0)
        if (semctl(semid, SEMNUM, IPC_RMID) == -1)
            perror("delete semaphores failed");
    pthread_mutex_destroy(&queueMutex);
    pthread_mutex_destroy(&countMutex);
    pthread_mutex_destroy(&fileMutex);
}

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

void WriteToFileInternalCount(int fd)
{
    int temp = GetInternalCount();
    char buff[100];
    sprintf(buff, "thread identifier is %u and internal_count is %d\n",
            pthread_self(), temp);
    if (write(fd, buff, strlen(buff)) == -1)
    {
        perror("write error");
        exit(EXIT_FAILURE);
    }
}

void* threadFunc(void *arg)
{
    int fd = *(int *) arg, x, amount, i;
    char action;
    struct timespec t;

    //TODO remove this crap
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1)
    {
        amount = 0;
        action = Dequeue(); //get the job
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
            case 'f':
                WriteToFileInternalCount(fd);
                break;
            case 'g':
                for (i = 0; i < THREADPOOL_SIZE; ++i)
                    if (threadPool[i] != pthread_self())
                        if (pthread_cancel(threadPool[i]) != 0)
                            perror("error closing thread");
                WriteToFileInternalCount(fd);
                if (close(fd) != 0)
                    perror("close error g");
                exit(EXIT_SUCCESS);
            case 'h':
                for (i = 0; i < THREADPOOL_SIZE; ++i)
                    if (threadPool[i] != pthread_self())
                        if (pthread_join(threadPool[i], NULL) != 0)
                            perror("error joining thread");
                WriteToFileInternalCount(fd);
                if (close(fd) != 0)
                    perror("close error h");
                exit(EXIT_SUCCESS);
                break;
        }
        // for letters a to e
        x = rand() % 91 + 10; // 10<=x<=100
        t.tv_nsec = x;
        t.tv_sec =0;
        nanosleep(&t, NULL);
        AddToInternalCount(amount);
    }
#pragma clang diagnostic pop
}

int main()
{
    int             fd, shmid = -1, i;
    key_t           key;
    struct sembuf   sops[SEMNUM];
    union semun     semarg;

    semarg.array = (unsigned short *) malloc(SEMNUM);
    if (semarg.array == NULL) {
        perror("malloc error 1");
        exit(EXIT_FAILURE);
    }

    if ((fd = open(FILE_NAME, O_CREAT | O_WRONLY,
                   S_IRUSR | S_IWUSR | S_IRGRP)) == -1)
    {
        perror("open error");
        exit(EXIT_FAILURE);
    }

    jobQueue = (Queue *) malloc(sizeof(Queue));
    if (jobQueue == NULL)
    {
        perror("malloc error");
        exit(EXIT_FAILURE);
    }
    if (atexit(atExitFunc) == -1)
        perror("atexit error");

    /* get key to shared memory */
    if ((key = ftok(KEY_FILE, KEY_CHAR)) == -1) {
        perror("ftok error");
        exit(EXIT_FAILURE);
    }

    /* grab the shared memory created by server: */
    if ((shmid = shmget(key, SHM_SIZE, 0)) == -1) {
        perror("shmget error");
        exit(EXIT_FAILURE);
    }

    /* attach to the segment to get a pointer to it: */
    data = shmat(shmid, NULL, 0);
    if (data == (char *)(-1)) {
        perror("shmat error");
        exit(EXIT_FAILURE);
    }

    /* Creating the read&write semaphores */
    if ((semid=semget (key, SEMNUM , 0600 )) < 0)
    {
        perror("semget error");
        if (shmdt(data) == -1) {
            perror("shared memory detach error");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&queueMutex, PTHREAD_MUTEX_ERRORCHECK) != 0)
    {
        perror("queue mutex error");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&countMutex, PTHREAD_MUTEX_ERRORCHECK) != 0)
    {
        perror("count mutex error");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&fileMutex, PTHREAD_MUTEX_ERRORCHECK) != 0)
    {
        perror("file mutex error");
        exit(EXIT_FAILURE);
    }

    //setting values
    semarg.array[SEM_READ] = 0;
    semarg.array[SEM_WRITE] = 0;
    if (semctl(semid, SEMNUM, SETALL, semarg) == -1)
    {
        perror("SETALL error");
        exit(EXIT_FAILURE);
    }

    for (i =0; i < THREADPOOL_SIZE; ++i)
        pthread_create(&(threadPool[i]), NULL, threadFunc, NULL); //TODO func
    // call

    sops[0].sem_flg = 0;

    //TODO delete this crap
    /* start reading the shared memory until finished by another thread */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
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

        Enqueue(temp);
    }
#pragma clang diagnostic pop
}