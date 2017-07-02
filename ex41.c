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

#define KEY_FILE "318459450.txt" //file for key
#define KEY_CHAR 'T' //char for key
#define SHM_SIZE 1024 //size of shared mem
#define SEMNUM 3 //num of semaphores
#define SEM_WRITE 0 //index of write semaphore
#define SEM_READ 1  //index of read semaphore
#define LOCK -1 //the sem_op for lock
#define UNLOCK 1 //the sem_op for unlock

/*//for using sempahores
union semun {
    int val;
    struct semid_ds *buf;
    ushort *array;
};*/


int main() {
    int             shmid;
    key_t           key;
    char            *data, action;
    int             semid;
//    union semun     semarg;
    struct sembuf   sops[SEMNUM];

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

    /* grabbing the semaphores created by the server */
    if ((semid=semget (key, SEMNUM , 0600 )) < 0)
    {
        perror("semget error");
        if (shmdt(data) == -1) {
            perror("shared memory detach error");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }

    sops->sem_flg = 0;
    do {
        printf("Please enter request code\n");
        scanf("%c\n", &action);
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
            perror("error locking write semaphore");
            if (shmdt(data) == -1) {
                perror("shared memory detach error");
            }
        }

        *data = action; //write to shared mem

        sops->sem_op = UNLOCK;
        sops->sem_num = SEM_READ;
        if (semop(semid, sops, 1) < 0) //unlock the read semaphore
        {
            perror("error unlocking read semaphore");
            if (shmdt(data) == -1) {
                perror("shared memory detach error");
            }
        }
        //now to next action
    } while (1);

    //detaching from shared memory
    if (shmdt(data) == -1) {
        perror("shared memory detach error");
        /*if (semctl(semid, SEMNUM, IPC_RMID) == -1 && errno != EPERM)
        {
            perror("delete semaphore error");
        }*/
        exit(EXIT_FAILURE);
    }

    /*if (semctl(semid, SEMNUM, IPC_RMID) == -1 && errno != EPERM)
    {
        perror("delete semaphore error");
        exit(EXIT_FAILURE);
    }*/

    exit(EXIT_SUCCESS);
}