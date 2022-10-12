/*processo USER: invia transazioni*/

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/shm.h>
#include <sys/sem.h>
#include <wait.h>
#include <time.h>
#include <math.h>
#include <sys/msg.h>

//-------------------------------- var globali
conf *conf_ptr;
libromastro *libro_ptr;
int *numLett_ptr;
pid_t *users_pid;
pid_t *nodes_pid;
//lista_nodi nodes_pid=NULL;//nodi amici
//lista_nodi last_node;
//nodo *n;
int shm_conf_id, shm_libro_id, shm_numLett_id, shm_listaUsers_id, shm_listaNodes_id, msq_id, sem_id,msq_nodi_p_id; 
int soldi_utente_inviati=0, bil_val,fallimenti_consec;

void my_IPC_access();
int mio_bilancio_corrente();
void invio_transaz();
void aggancio_shm_ptrs();
void inizio_lettura();
void fine_lettura();
void sigurs1_hendler(int signum);


int main(int argc, char *argv[]){    
    //printf("[User: %ld]\n",(long)getpid());
    //--------------------------recupero l'accesso agli oggetti IPC 
    my_IPC_access(); 
    
    //--------------------------decremento notifico il master che io sono pronto
    if(reserveSem(sem_id,SEM_PRONTI_TUTTI)==-1) errExit("[User]: ho fallito la reserveSem");
    /*else printf("[User - PID = %ld]: ho decrementato il semaforo 1\n",(long) getpid());*/

    //--------------------------mi metto in attesa dello 0 per il "liberi tutti" dato dal master*/  
    /*printf("[User - PID = %ld]: mi metto in attesa dello 0 del sem 0\n",(long) getpid());*/
    if(semOp(sem_id,SEM_LIBERI_TUTTI,0,0600) == -1) errExit("[User]: ho fallito la semOp 0");//attendo 0

    //--------------------------ho ottenuto la CPU! 
    //ora ho la certezza che il master abbia finito di creare tutti i processi nodo e utente
    // e posso estrarre i nodo-user destinatari della transizione
    aggancio_shm_ptrs();
    //nodes_pid=calloc(conf_ptr->SO_NODES_NUM,sizeof(pid_t));
    
    /*--------------------------riempo lista nodi!
    for(int i=0;i<conf_ptr->SO_NODES_NUM;i++)
    {
        msg_node_pid msg;
        if((msgrcv(msq_nodi_p_id, &msg, sizeof(msg_node_pid)-(sizeof(long)), getpid(),0))== -1)
            errExit("[User] msgrcv error");
        if(i==0)
        {
          nodes_pid = last_node = (nodo *)malloc(sizeof(nodo));
          last_node->pid=msg.p;
          last_node->next=NULL;
        }        
        else{
          n = (nodo *)malloc(sizeof(nodo));
          n->pid=msg.p;
          n->next = NULL;
          last_node->next=n;
          last_node = n;
        }
        //printf("\n[User: %d] i:%d, pid:%d\n",getpid(),i,(int)msg.p);
    }*/

    //------------------------------ ricezione segnale capienza massima libro mastro raggiunta da un nodo
    if(signal(SIGUSR1,sigurs1_hendler)==SIG_ERR) errExit("[User]: ho fallito nella signal SIGUSR1");

    fallimenti_consec=0;
    //--------------------------calcolo il bilancio 
    while (fallimenti_consec<conf_ptr->SO_RETRY)
    {
        bil_val = mio_bilancio_corrente();
        //printf("[User - PID = %ld]: bilancio: %d\n",(long) getpid(),bil_val);
        if(bil_val>=2) 
        {//--------------------------invio una transaz.
            invio_transaz();
        }
        else fallimenti_consec++;
        
        //-----------------------attendo intervallo temporale
        struct timespec tim, tim2;
        tim.tv_sec = 0;  tim.tv_nsec = estrai_tra(conf_ptr->SO_MIN_TRANS_GEN_NSEC,conf_ptr->SO_MAX_TRANS_GEN_NSEC);
        nanosleep(&tim,&tim2);
    }
    
    exit(EXIT_SUCCESS);
}

/* ------------------------------------ recupero l'accesso agli oggetti IPC-------------------------------------------*/
void my_IPC_access(){
    if((msq_id = msgget(KEY_MSGQ, 0600)) == -1) errExit("[User]: ho fallito la msgget");
    if((msq_nodi_p_id = msgget(KEY_MSGQ_NODI_PID, 0600)) == -1) errExit("[User]: ho fallito la msgget nodi_p");
	if((sem_id = semget(KEY_SEM, 0, 0600)) == -1) errExit("[User]: ho fallito la semget");
    if((shm_conf_id = shmget(KEY_SHM_CONF, 0, 0666)) == -1) errExit("[Node]: ho fallito la shmget conf");      
    if((conf_ptr = shmat(shm_conf_id,  NULL, SHM_RDONLY)) == (void *)-1) errExit("[Node]: ho fallito nella shmat conf_ptr");
    if((shm_libro_id = shmget(KEY_SHM_LIBRO, sizeof(libromastro), 0600)) == -1) errExit("[User]: ho fallito la shmget 1");
    if((shm_listaUsers_id = shmget(KEY_SHM_PROC, 0, 0666)) == -1) errExit("[User]: ho fallito la shmget 2");  
    if((shm_listaNodes_id = shmget(KEY_SHM_NODES, 0, 0666)) == -1) errExit("[User]: ho fallito la shmget 2");    
    if((shm_numLett_id = shmget(KEY_SHM_NUMLETT, sizeof(int), 0666)) == -1) errExit("[User]: ho fallito la shmget 3");    
}

void aggancio_shm_ptrs()//ora l-proc contiene tutti i pid utenti e nodi
{// tutti i processi devono solo leggere questi dati --> no sezione critica
    /* ----------------------------------------------- aggancio del segmento users_pid  */
    if ((users_pid = shmat(shm_listaUsers_id, NULL, SHM_RDONLY)) == (void *)-1)
        errExit("[User]: ho fallito nella shmat");
    //else    printf("\n ---> u:%d - n:%d\n ",l_proc_ptr->users_pids[0],l_proc_ptr->nodes_pids[0]);
     /* ----------------------------------------------- shm proc detach 
    if (shmdt(l_proc_ptr) == -1) //else: 0 success
        errExit("[Master]: ho fallito la shmdt");*/

    /* ----------------------------------------------- aggancio del segmento nodes_pid  */
    if ((nodes_pid = shmat(shm_listaNodes_id, NULL, SHM_RDONLY)) == (void *)-1)
        errExit("[User]: ho fallito nella shmat");
    //printf("[User : %d] nodes_pid[0] = %d, users_pid[1] = %d\n",getpid(),nodes_pid[0],users_pid[1]);

    //----------------------------------------------- mi aggancio  al libro mastro in sola lettura
    if((libro_ptr = (libromastro *)shmat(shm_libro_id,NULL,SHM_RDONLY))==(void *)-1)
        errExit("[User]: ho fallito nella shmat libroMastro");  

    //----------------------------------------------- mi aggancio a numLett
    if((numLett_ptr = (int *)shmat(shm_numLett_id,NULL,0))==(int *)-1)
        errExit("[User]: ho fallito nella shmat numLett"); 
}

/* ---------------------------------------------- BILANCIO CORRENTE -----------------------------------------------*/
int mio_bilancio_corrente()
{
    int bil = conf_ptr->SO_BUDGET_INIT;      

    //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> PROBLEMA LETTORI-SCRITTORI: gestito con inizio_lettura(); fine_lettura();
    //piu utenti possono leggere contemporaneamente il libro.
    inizio_lettura();        
    //--------------------------leggo libro
    for(int i=0; i<libro_ptr->id_blocco_corrente; i++)// sizeof(libro_ptr->vect_blocchi[i].vect_trans) != 0
    {   //int n=sizeof(libro_ptr->vect_blocchi[i].vect_trans);
        //libro_ptr->vect_blocchi[i].vect_trans[j].sender != nullptr
        for(int j=0; j<SO_BLOCK_SIZE-1; j++) 
        {
            //se è una mia transizione
            if(libro_ptr->vect_blocchi[i].vect_trans[j].receiver == getpid())
                bil += libro_ptr->vect_blocchi[i].vect_trans[j].quantita;
        }
    }
    fine_lettura();
    //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<   

    //----------------------------------------------- sottraggo importi transizioni spedite (confermate e non)
    bil-=soldi_utente_inviati;

    return bil;
}

void inizio_lettura()
{
    // Disattivo SIGUSR1
    sigset_t old_mask;
	old_mask = block_signals(1, SIGUSR1);

    if(reserveSem(sem_id,SEM_MUTEX)==-1) errExit("[User]: ho fallito la reserve su SEM_MUTEX");
    //******************* SEZIONE CRITICA 
    *numLett_ptr+=1;
    //printf("[User - PID = %ld]: \t\t\tnumLett: %d\n",(long) getpid(),*numLett_ptr);
    if(*numLett_ptr==1)//se non sono il primo lettore, un user prima di me avrà già svolto questa operzione
    {//se sto leggendo il libro devo bloccare i nodi che vogliono scrivervi
        if(reserveSem(sem_id,SEM_SCRIVI)==-1) errExit("[User]: ho fallito la reserve su SEM_SCRIVI");
        //printf("[User - PID = %ld]: \t\t\tHO ACQUISITO SCRIVI\n",(long)getpid());
    }
    //******************** END   
    if(releaseSem(sem_id,SEM_MUTEX)==-1) errExit("[User]: ho fallito la release su SEM_MUTEX");

    reset_signals(old_mask);
}

void fine_lettura()
{
    // Disattivo SIGUSR1
    sigset_t old_mask;
	old_mask = block_signals(1, SIGUSR1);

    if(reserveSem(sem_id,SEM_MUTEX)==-1) errExit("[User]: ho fallito la reserve su SEM_MUTEX");
    //******************* SEZIONE CRITICA 
    *numLett_ptr-=1;
    //printf("[User - PID = %ld]: \t\t\tnumLett: %d\n",(long) getpid(),*numLett_ptr);
    if(*numLett_ptr==0)//se non ci sono utenti che stanno leggendo il libro
    {//sblocco nodi che vogliono scrivervi
        if(releaseSem(sem_id,SEM_SCRIVI)==-1) errExit("[User]: ho fallito la reserve su SEM_SCRIVI");
        //printf("[User - PID = %ld]: \t\t\tHO RILASCIATO SCRIVI\n",(long)getpid());
    }
    //******************** END   
    if(releaseSem(sem_id,SEM_MUTEX)==-1) errExit("[User]: ho fallito la release su SEM_MUTEX");

    reset_signals(old_mask);
}

/* ---------------------------------- INVIO TRANSAZIONE A NODO -----------------------------------------------*/
void invio_transaz()
{   
    int randomUser;//---------------------estraggo uno user diverso da me
    do{randomUser = estrai_tra(0,conf_ptr->SO_USERS_NUM-1);}while(randomUser ==getpid());
    int randomNode = estrai_tra(0,conf_ptr->SO_NODES_NUM-1);//l_nodes_count(nodes_pid)-1
    int estratto = estrai_tra(2,bil_val);
    //---------------------------msg
    msg_trans new_trans;
    //--------------------------imposto nodo ricevitore
    new_trans.mtype = (long)nodes_pid[randomNode];//get_pid_node_index(nodes_pid,randomNode);
    new_trans.hops = conf_ptr->SO_HOPS;
     //--------------------------prelevo il timestamp in nanosec
    struct timespec t;
    if(clock_gettime(CLOCK_REALTIME, &t)==-1) errExit("[User]: ho fallito nella clock_gettime in invio_transaz()");
    new_trans.t.timestamp = t.tv_nsec;
    //--------------------------imposto user mittente e user destinatario della transazione
    new_trans.t.sender = getpid();
    new_trans.t.receiver =users_pid[randomUser];
    //---------------------------elaboro reward e di conseguenza importo (quantità)
    int perc = estratto * conf_ptr->SO_REWARD /100;
    new_trans.t.reward = (round(perc)<1) ? 1 : round(perc);// arrotondo e imposto minimo 1
    new_trans.t.quantita = estratto - new_trans.t.reward;  
    //---------------------------invio transizione al nodo
    if(msgsnd(msq_id, &new_trans, sizeof(new_trans)-sizeof(long), IPC_NOWAIT)== -1)//IPC_NOWAIT
        fallimenti_consec++;
    //senza IPC_NOWAIT se una coda di msg è piena, msgsnd() si blocca finché non si libera abbastanza spazio per il
    //messaggio che si desidera aggiungere
        //errExit("[User]: ho fallito nella msgsnd in invio_transaz()");
    /*if(msgsnd(msq_id, &new_trans, sizeof(new_trans)-sizeof(long), IPC_NOWAIT)== -1)
    {
        fallimenti_consec++;
        while(msgsnd(msq_id, &new_trans, sizeof(new_trans)-sizeof(long), IPC_NOWAIT)==-1 && fallimenti_consec<conf_ptr->SO_RETRY)
        {   
            struct timespec tim, tim2;
            tim.tv_sec = 0;  tim.tv_nsec = 100000;
            nanosleep(&tim,&tim2);
        }
    }*/
    else{
        //--------------------------AVVISO NODO RICEVITORE CON UN SEGNALE SIGURS1!
        //kill(new_trans.mtype,SIGUSR1);
        soldi_utente_inviati+=new_trans.t.quantita + new_trans.t.reward;
        fallimenti_consec=0;
    }
}

void sigurs1_hendler(int signum){//seganle da terminale o da master per generare una nuova transizione
    bil_val = mio_bilancio_corrente();
    if(bil_val>=2) 
    {//--------------------------invio una transaz.
        invio_transaz();
        printf("( [User %d] : transizione inviata inseguito alla ricezione di un segnale )\n\n",getpid());
    }
    //-----------------------attendo intervallo temporale
    struct timespec tim, tim2;
    tim.tv_sec = 0;  tim.tv_nsec = estrai_tra(conf_ptr->SO_MIN_TRANS_GEN_NSEC,conf_ptr->SO_MAX_TRANS_GEN_NSEC);
    nanosleep(&tim,&tim2);
}
