#include <sys/sem.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include "common.h"

/* ------------------------------ INITIALIZE SEMAPHORE TO 0 ("in use") ------------------------------------------------------*/
int initSemWithValue(int semId, int semNum, int semVal){
    union semun {   int val;   struct semid_ds* buf;   unsigned short* array;
        #if defined(__linux__)
            struct seminfo* __buf;
        #endif
    } arg;

    arg.val=semVal;
    return semctl(semId,semNum,SETVAL,arg);    //SETVAL Il valore del semaforo semnum nel set 
    //                                  riferito da semid è inizializzato al valore arg.val.
}

/* ------------------------------ RESERVE SEMAPHORE - DECREMENT IT BY 1 -----------------------------------------------------*/
int reserveSem(int semId, int semNum)
{
    struct sembuf sop; // la struttura che definisce l'operazione
    sop.sem_num = semNum; // specifica il semNum semaforo nel set
    sop.sem_op = -1;//se sem_op è < 0, semop() decrementa il valore del semaforo del valore specicato da sem_op
    sop.sem_flg = 0; // NON settiamo flag per effettuare operazioni speciali

    return semop(semId, &sop, 1);//int semop(int semid, struct sembuf *sops, unsigned int nsops)
    //                      nsops è la dimensione dell'arrey sops delle operazioni da eseguire
}

/* ------------------------------ RELEASE SEMAPHORE - INCREMENT IT BY 1 -----------------------------------------------------*/
int releaseSem(int semId, int semNum)
{
    struct sembuf sop; // la struttura che definisce l'operazione
    sop.sem_num = semNum; // specifica il semNum semaforo nel set
    sop.sem_op = 1;//se sem_op è > 0, semop() incrementa il valore del semaforo del valore specicato da sem_op
    sop.sem_flg = 0; // NON settiamo flag per effettuare operazioni speciali

    return semop(semId, &sop, 1);//int semop(int semid, struct sembuf *sops, unsigned int nsops)
    //                      nsops è la dimensione dell'arrey sops delle operazioni da eseguire
}

/* ----------------------------------------- EXECUTE A SEMOP -------------------------------------------------------------*/
int semOp(int s_id, unsigned short sem_num, short sem_op, short sem_flg)
{
    struct sembuf sops;
	sops.sem_flg = sem_flg;
	sops.sem_op = sem_op;
	sops.sem_num = sem_num;
	return semop(s_id, &sops, 1);
}

/* ----------------------------------------- stampa valore sem -----------------------------------------------------------*/
void sem_stampaVal(int s_id, unsigned short sem_num)//funzione usata principalmente per testare il programma
{
    int val;
    if((val=semctl(s_id,sem_num,GETVAL))==-1)
        errExit("[----]: ho fallito la semctl GETVAL");
    else printf("[--- SEM %d  val: %d ---]\n",sem_num,val);
}

/* ----------------------------------------- valore casuale tra -----------------------------------------------------------*/
long estrai_tra(long min,long max)
{
    //srand(time(NULL)); //RESTITUISCE SECONDI --> PIU PROCESSI ESEGUITI NELLO STESSO SECONDO --> GENERA STESSE SEQUENZE 
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    srand(t.tv_nsec);
    return rand() % (max + 1 - min) + min;
}

/* ----------------------------------------- conta nodi in lista ----------------------------------------------------------*/
int l_nodes_count(lista_nodi l)
{
    int count=0;
    while(l!=NULL) {   
	  count++;
    l=l->next;
    }
    return count;
}

/* ---------------------------- restituisce il pid del nodo index della lista ---------------------------------------------*/
int get_pid_node_index(lista_nodi l,int i)
{
    while(i>0)
    {
        l=l->next;
        i--;
    }
    return l->pid;
}

/* ----------------------------------------- conta nodi in lista ----------------------------------------------------------*/
int l_nodes_info_count(lista_nodi_info l)
{
    int count=0;
    while(l!=NULL) {   
	  count++;
    l=l->next;
    }
    return count;
}
/* ---------------------------- restituisce il pid del nodo index della lista ---------------------------------------------*/
int get_pid_node_info_index(lista_nodi_info l,int i)
{
    while(i>0)
    {
        l=l->next;
        i--;
    }
    return l->pid;
}

/* --------- restituisce il campo n(numero di nodi in pool a termine simulazione) del nodo index della lista ---------------------------------*/
int get_n_node_info_index(lista_nodi_info l,int i)
{
    while(i>0)
    {
        l=l->next;
        i--;
    }
    return l->n;    
}



//******************************* codice tratto da moodle come specificato nel .h
sigset_t block_signals(int count, ...) {
	sigset_t mask, old_mask;
	va_list argptr;
	int i;

	sigemptyset(&mask);

	va_start(argptr, count);

	for (i = 0; i < count; i++) {
		sigaddset(&mask, va_arg(argptr, int));
	}

	va_end(argptr);

	sigprocmask(SIG_BLOCK, &mask, &old_mask);
	return old_mask;
}

sigset_t unblock_signals(int count, ...) {
	sigset_t mask, old_mask;
	va_list argptr;
	int i;

	sigemptyset(&mask);

	va_start(argptr, count);

	for (i = 0; i < count; i++) {
		sigaddset(&mask, va_arg(argptr, int));
	}

	va_end(argptr);

	sigprocmask(SIG_UNBLOCK, &mask, &old_mask);
	return old_mask;
}

void reset_signals(sigset_t old_mask) {
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
}
