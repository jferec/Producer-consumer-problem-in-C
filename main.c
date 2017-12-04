#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdbool.h>
#include <wait.h>
#include <time.h>

#define BUFFER_SIZE 9
#define LOOPS 1000

void consumerA();
void consumerB();
void consumerC();
void producer();

enum {EMPTY, FULL, MUTEX_AC, MUTEX_B};


typedef struct Element{
    int value;
    bool readbyA;
    bool readbyB;
    bool readbyC;
}Element;

Element create_item(int value)
{
    Element a;
    a.readbyA = a.readbyB = a.readbyC = false;
    a.value = value;
    return a;
}


int shmid, semid, i;
Element *buffer;

int main(){
    srand(time(NULL));

    pid_t pid;
/*********************SHARED MEMORY FRAGMENT*************************/

    shmid = shmget(IPC_PRIVATE, (BUFFER_SIZE + 2)* sizeof(Element), IPC_CREAT| 0600);
    if (shmid == -1) {
        perror("Utworzenie segmentu pamieci wspoldzielonej");
        exit(1);
    }

    buffer = (Element*)shmat(shmid, NULL, 0);
    if (buffer == NULL) {
        perror("Przylaczenie segmentu pamieci wspoldzielonej");
        exit(1);
    }

#define indexZ buffer[BUFFER_SIZE]//element przechowujacy index do zapisu
#define indexO buffer[BUFFER_SIZE+1]//element przechowujacy index do odczytu

    indexZ = create_item(0);
    indexO = create_item(0);

    shmdt(buffer);
    semid = semget(IPC_PRIVATE, 4, IPC_CREAT|IPC_EXCL|0600);
    if(semid == -1) {
        semid = semget(IPC_PRIVATE, 4, 0600);
        if (semid == -1) {
            perror("Utworzenie tablicy semaforow");
            exit(1);
        }

    }
/*********************SEMAPHORES INIT**************************/
    else {
        if (semctl(semid, EMPTY, SETVAL, (int) BUFFER_SIZE) == -1) {
            perror("Nadanie wartosci semaforowi EMPTY");
            exit(1);
        }
        if (semctl(semid, FULL, SETVAL, (int) 0) == -1) {
            perror("Nadanie wartosci semaforowi FULL");
            exit(1);
        }
        if (semctl(semid, MUTEX_AC, SETVAL, (int) 1) == -1) {
            perror("Nadanie wartosci semaforowi MUTEXAC");
            exit(1);
        }
        if (semctl(semid, MUTEX_B, SETVAL, (int) 1) == -1) {
            perror("Nadanie wartosci semaforowi MUTEXB");
            exit(1);
        }

    }
/*********************PROCESY***********************/
    int i = 5;
    if((pid = fork()) < 0){
        perror("fork");
        abort();
    }
    else if(pid == 0){
        producer();
        srand(time(NULL)+ i);
        exit(0);
    }


    if((pid = fork()) < 0){
        perror("fork");
        abort();
    }
    else if(pid == 0){
        consumerA();
        srand(time(NULL) + 2*i);
        exit(0);
    }

    puts("2");
    if((pid= fork()) < 0){
        perror("fork");
        abort();
    }
    else if(pid == 0){
        consumerB();
        srand(time(NULL) + 3*i);
        exit(0);
    }
    puts("3");
    if((pid = fork()) < 0){
        perror("fork");
        abort();
    }
    else if(pid == 0){
        consumerC();
        srand(time(NULL) + 4*i);
        exit(0);
    }

    while(wait(NULL)>0);

    free(buffer);

    return 0;

}


void up(unsigned short semnum){
    struct sembuf sops;
    sops.sem_num = semnum;
    sops.sem_op = 1;
    sops.sem_flg = 0;
    if (semop(semid, &sops, 1) == -1){
        perror("Podnoszenie semafora");
        exit(1);
    }
}

void down(unsigned short semnum){
    struct sembuf sops;
    sops.sem_num = semnum;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    if (semop(semid, &sops, 1) == -1){
        perror("Opuszczanie semafora");
        exit(1);
    }
}



void producer() {
    for(int j = 0; j<LOOPS; j++) {

        down(0);//EMPTY
        down(2);//MUTEX_AC
        down(3);//MUTEX_B

        buffer = (Element*)shmat(shmid, NULL, 0);

        printf("Produkuje %d-ty element\n", j + 1);
        usleep(1000);
        buffer[indexZ.value] = create_item(j);
        indexZ.value = (indexZ.value + 1) % BUFFER_SIZE;

        shmdt(buffer);

        up(3);//MUTEX_B
        up(2);//MUTEX_AC
        up(1);//FULL


    }
    usleep(rand()%1000);
}

void consumerA() {
    for (int j = 0; j < LOOPS; j++) {
/*********************READER**************************/
        down(1);//FULL**********SPRAWDZENIE CZY BUFOR JEST PUSTY
        down(2);//MUTEX_AC
        up(1);//FULL************SPRAWDZENIE CZY BUFOR JEST PUSTY

        buffer = (Element*)shmat(shmid, NULL, 0);
        if(buffer == NULL) exit(12);
        if (buffer[indexO.value].readbyA == false && buffer[indexO.value].readbyC == false) {
            buffer[indexO.value].readbyA = true;
            usleep(1000);
            printf("A przeczytal %d-ty element na indexie: %d\n",buffer[indexO.value].value+1, indexO);
        }
        shmdt(buffer);
        up(2);//MUTEX_AC
/*********************READER END**************************/
/*********************CONSUMER**************************/
        down(1);//FULL
        down(2);//MUTEX_AC
        down(3);//MUTEX_B

        buffer = (Element*)shmat(shmid, NULL, 0);
        if (buffer[indexO.value].readbyB == true && (buffer[indexO.value].readbyA == true || buffer[indexO.value].readbyC == true)) {
            printf("A zjada %d-ty element\n", buffer[indexO.value].value +1);
            indexO.value = (indexO.value + 1) % BUFFER_SIZE;
            usleep(rand()%1000);
            shmdt(buffer);
            up(3);//MUTEX_B
            up(2);//MUTEX_AC
            up(0);//EMPTY
        } else {
            shmdt(buffer);
            up(3);//MUTEX_B
            up(2);//MUTEX_AC
            up(1);//FULL
        }
/*********************CONSUMER END**************************/
        usleep(rand()%1000);
    }


}



void consumerB() {
    for (int j = 0; j < LOOPS; j++) {
/*********************READER**************************/
        down(1);//FULL**********SPRAWDZENIE CZY BUFOR JEST PUSTY
        down(3);//MUTEX_B
        up(1);//FULL************SPRAWDZENIE CZY BUFOR JEST PUSTY

        buffer = (Element*)shmat(shmid, NULL, 0);
        if(buffer == NULL) exit(12);
        if (buffer[indexO.value].readbyB == false) {
            buffer[indexO.value].readbyB = true;
            usleep(1000);
            printf("B przeczytal %d-ty element na indexie: %d\n",buffer[indexO.value].value+1, indexO);
        }
        shmdt(buffer);
        up(3);//MUTEX_B
/*********************READER END**************************/
/*********************CONSUMER**************************/
        down(1);//FULL
        down(2);//MUTEX_AC
        down(3);//MUTEX_B
        buffer = (Element*)shmat(shmid, NULL, 0);
        if ((buffer[indexO.value].readbyA == true || buffer[indexO.value].readbyC == true)&&buffer[indexO.value].readbyB == true) {
            printf("B zjada %d-ty element\n", buffer[indexO.value].value +1);
            indexO.value = (indexO.value + 1) % BUFFER_SIZE;
            usleep(1000);
            shmdt(buffer);
            up(3);//MUTEX_B
            up(2);//MUTEX_AC
            up(0);//EMPTY
        } else {
            shmdt(buffer);
            up(3);//MUTEX_B
            up(2);//MUTEX_AC
            up(1);//FULL
        }
/*********************CONSUMER END**************************/
        usleep(rand()%1000);
    }
}


void consumerC() {
    for(int j = 0; j<LOOPS; j++) {
/*********************READER**************************/
        down(1);//FULL**********SPRAWDZENIE CZY BUFOR JEST PUSTY
        down(2);//MUTEX_AC
        up(1);//FULL************SPRAWDZENIE CZY BUFOR JEST PUSTY

        buffer = (Element*)shmat(shmid, NULL, 0);
        if(buffer == NULL) exit(12);
        if (buffer[indexO.value].readbyA == false && buffer[indexO.value].readbyC == false) {
            buffer[indexO.value].readbyC = true;
            usleep(1000);
            printf("C przeczytal %d-ty element na indexie: %d\n",buffer[indexO.value].value+1, indexO);
        }
        shmdt(buffer);
        up(2);//MUTEX_AC
/*********************READER END**************************/
/*********************CONSUMER**************************/
        down(1);//FULL
        down(2);//MUTEX_AC
        down(3);//MUTEX_B

        buffer = (Element*)shmat(shmid, NULL, 0);
        if(buffer == NULL) exit(12);
        if(buffer[indexO.value].readbyB == true && (buffer[indexO.value].readbyA == true || buffer[indexO.value].readbyC == true)) {
            printf("C zjada %d-ty element\n", buffer[indexO.value].value +1);
            indexO.value = (indexO.value + 1) % BUFFER_SIZE;
            usleep(1000);
            shmdt(buffer);
            up(3);//MUTEX_B
            up(2);//MUTEX_AC
            up(0);//EMPTY

        } else {
            shmdt(buffer);
            up(3);//MUTEX_B
            up(2);//MUTEX_AC
            up(1);//FULL
        }
/*********************CONSUMER END**************************/
        usleep(rand()%1000);
    }

}
