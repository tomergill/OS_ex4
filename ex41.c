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
#include <errno.h>

#define FILE_NAME "./318459450.txt" //file for key
#define KEY_FILE  FILE_NAME         //file for key
#define KEY_CHAR  'T'               //char for key
#define SHM_SIZE  1024              //size of shared mem
#define SEMNUM    3                 //num of semaphores
#define SEM_WRITE 0                 //index of write semaphore
#define SEM_READ  1                 //index of read semaphore
#define LOCK     -1                 //the sem_op for lock
#define UNLOCK    1                 //the sem_op for unlock

/******************************************************************************
 * function name: main
 * The Input: None.
 * The output: Gets a char between 'a' to 'h' and "sends" it to the server,
 *      or exits if got 'i' ('g' and 'h' that closes the server also closes
 *      the client).
 * The Function operation: Opens the shared memory and the semaphores created
 *      by the server, and then until getting 'g', 'h' or 'i' sends the
 *      character got to the server (add +1 to the write semaphore, writes to
 *      the shared mem and decreases -1 from the read semaphore). 'g' and 'h'
 *      will be sent to server, 'i' will not.
*******************************************************************************/
int main()
{
    int             shmid;
    key_t           shmKey, semKey;
    char            *data, action, dummy;
    int             semid;
    struct sembuf   sops[SEMNUM];

    /* get shmKey to shared memory */
    if ((shmKey = ftok(KEY_FILE, KEY_CHAR)) == -1)
    {
        perror("ftok error");
        exit(EXIT_FAILURE);
    }

    /* grab the shared memory created by server: */
    shmid = shmget(shmKey, SHM_SIZE, 0666);
    if (shmid == -1)
    {
        perror("shmget error");
        exit(EXIT_FAILURE);
    }

    /* attach to the segment to get a pointer to it: */
    data = shmat(shmid, NULL, 0);
    if (data == (char *)(-1))
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

    /* grabbing the semaphores created by the server */
    if ((semid=semget (semKey, SEMNUM , 0600 )) < 0)
    {
        perror("semget error");
        if (shmdt(data) == -1)
            perror("shared memory detach error");
        exit(EXIT_FAILURE);
    }

    sops->sem_flg = 0;
    do
    {
        printf("Please enter request code\n");
        scanf("%c%c", &action, &dummy);
        action = tolower(action);
        if (action < 'a' || action > 'i')
            continue;
        if (action == 'i')
            break;

        /* write to shared memory */
        sops->sem_op = LOCK;
        sops->sem_num = SEM_WRITE;
        if (semop(semid, sops, 1) < 0) //lock the write semaphore
        {
            if (errno == EIDRM || errno == EINVAL)
                break; //server is dead
            perror("error locking write semaphore");
            if (shmdt(data) == -1)
            {
                perror("shared memory detach error");
            }
            exit(EXIT_FAILURE);
        }

        *data = action; //write to shared mem

        sops->sem_op = UNLOCK;
        sops->sem_num = SEM_READ;
        if (semop(semid, sops, 1) < 0) //unlock the read semaphore
        {
            if (errno == EIDRM || errno == EINVAL)
                break; //server is dead
            perror("error unlocking read semaphore");
            if (shmdt(data) == -1)
            {
                perror("shared memory detach error");
            }
            exit(EXIT_FAILURE);
        }

        if (action == 'g' || action == 'h')
            break;

    } while (1);

    //detaching from shared memory
    if (shmdt(data) == -1)
    {
        perror("shared memory detach error");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}