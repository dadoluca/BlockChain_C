/*DADONE LUCA - LIBRO MASTRO*/
#include "common.h"
#include "defaultConf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <wait.h>
#include <time.h>
#include <math.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>

//--------------------- var globali
lista_nodi_info nodes_info;
lista_nodi_info last_node;
node_info *n;
pid_t *nodes_pid;
pid_t *users_pid;
libromastro *libro_ptr;
int *numLett_ptr;
conf *conf_ptr;
int shm_conf_id, shm_libro_id, shm_numLett_id, shm_listaUsers_id, shm_listaNodes_id, sem_id, msq_id,msq_nodi_p_id,msq_trPoolCount_id; 
int current_user,blocca_current_user=0;//conta user in vita

void lettura_parametri_stdin();
void shm_get_at();
void setSEM();
void msgq();
void creaUsers();
void creaNodes();
void mandaNodi();
void inizio_lettura();
void fine_lettura();
void stampa(int riepilogo);
void sigalrm_handler(int signum);
void sigchld_handler(int signum);
void sigurs1_hendler(int signum);
void sigurs2_hendler(int signum);
void arresto(int motivo);
void got_trans(msg_trans new_trans);
int* estrai_aCaso_tranne(int a,int dime);

int main(int argc, char *argv[])
{   
    /* ----------------------- creazione un segmento di SHM atto a contenere parametri di conf --------------------------*/                                                                     
    if ((shm_conf_id = shmget(KEY_SHM_CONF, sizeof(conf*), IPC_CREAT | 0666)) == -1)
        errExit("[Master]: ho fallito nella shmget conf_ptr");
    /* ------------------------------------------ aggancio del segmento -------------------------------------------------*/
    if ((conf_ptr = shmat(shm_conf_id, NULL, 0)) == (void *)-1)
        errExit("[Master]: ho fallito nella shmat conf_ptr");
    //------------------------------ leggo PARAMETRI di configurazione a TEMPO DI ESECUZIONE
    //conf_ptr=malloc(sizeof(conf));
    lettura_parametri_stdin();

    //------------------------------ ora posso fare l'inizializzazione
    users_pid=calloc(conf_ptr->SO_USERS_NUM,sizeof(pid_t));
    nodes_pid=calloc(conf_ptr->SO_NODES_NUM,sizeof(pid_t));
    shm_get_at(); 
    setSEM();
    msgq();

    printf("\n[Master - PID = %ld]: AVVIO SIMULAZIONE !\n\n", (long)getpid());
    //------------------------------ dopo SO_SIM_SEC terminerà la simulazione
    if(signal(SIGALRM,sigalrm_handler)==SIG_ERR) errExit("[Master]: ho fallito nella signal SIGALRM");
    alarm(conf_ptr->SO_SIM_SEC);
    //------------------------------ ricezione segnale in seguito a terminazione di uno user
    if(signal(SIGCHLD,sigchld_handler)==SIG_ERR) errExit("[Master]: ho fallito nella signal SIGCHLD");
    //------------------------------ ricezione segnale capienza massima libro mastro raggiunta da un nodo
    if(signal(SIGUSR1,sigurs1_hendler)==SIG_ERR) errExit("[Master]: ho fallito nella signal SIGUSR1");
        //------------------------------ ricezione segnale di invio da parte di un nodo di una transazione scartata
    if(signal(SIGUSR2,sigurs2_hendler)==SIG_ERR) errExit("[Master]: ho fallito nella signal SIGUSR2");

    //------------------------------- genero i miei processi 
    creaNodes();
    creaUsers();
    //printf("[MASTER] shm_conf_id: %d\n",shm_conf_id);   
    //printf("[MASTER] conf->SO_USER_NUM: %d\n",conf_ptr->SO_USERS_NUM);

    //------------------------------ semop di pronti tutti: attendo che ogni processo sia stato creato, inizializzato
    //------------------------------ e si sia messo in attesa del comando liberi tutti (per massimizzare concorrenza)
    if(semOp(sem_id,SEM_PRONTI_TUTTI,0,0600) == -1) errExit("[Master]: ho fallito la semOp SEM_PRONTI_TUTTI");//attendo 0

    mandaNodi();
    //------------------------------ liberi tutti! restituito il controllo al master do il via
    if(reserveSem(sem_id,SEM_LIBERI_TUTTI)==-1) errExit("[Master]: ho fallito la reserveSem di SEM_PRONTI_TUTTI");
    //----------------------- finchè vi sono utenti attivi
    while (current_user>0)
    {
        //----------------------- stampa a terminale
        inizio_lettura();//processo master si comporta da lettore del libro master
        stampa(STAMPA_PERIODICA);
        //stampare budjet corrente di ogni nodo e di ogni utente così come registrato nel mastro
        fine_lettura();
        printf("\n");

        //----------------------- ogni secondo invio ad uno user scelto a caso un segnale così 
        //----------------------- implementare il sistema di invio transiz. in risposta ad un segnale
        kill(users_pid[estrai_tra(0,conf_ptr->SO_USERS_NUM-1)],SIGUSR1);

        //-----------------------attesa inattiva
        struct timespec request, remaining;
        request.tv_sec = 1;  request.tv_nsec =0;
        while (nanosleep(&request, &remaining) == -1)
        {
            if(errno == EINTR) //interrotta dall'hendeler di un segnale
            {//riposa la rimanente parte di attesa interrotta
                request = remaining;
            }
        }
    }
    arresto(ALL_USERS_TERMINATED);

    exit(EXIT_SUCCESS);
}

/* ------------------------------ leggo PARAMETRI di configurazione a TEMPO DI ESECUZIONE ------------------------------*/
void lettura_parametri_stdin()
{
    int modalita_inserimento;
    printf("Conigurazione parametri: \n- Digitare 0 per usare i valori di default scelti dal programmatore. \n- Digitare 1 per usare i valori di esempio della consegna: conf1.\n- Digitare 2 per usare i valori di esempio della consegna: conf2.\n- Digitare 3 per usare i valori di esempio della consegna: conf3.\n"
    "- DIGITARE 4 PER L'INSERIMENTO MANUALE DEI PARAMETRI.\n");
    scanf("%d", &modalita_inserimento);           
    switch(modalita_inserimento)
    {
//CONF0 valori neutrali, a metà tra le conf proposte, utilizzati di solito da me per testing 
        case 0://------------------------------------------- inserimento automatico
            conf_ptr->SO_USERS_NUM = DEFAULT_SO_USERS_NUM;		                
            conf_ptr->SO_NODES_NUM = DEFAULT_SO_NODES_NUM;		
            conf_ptr->SO_BUDGET_INIT = DEFAULT_SO_BUDGET_INIT;		
            conf_ptr->SO_REWARD = DEFAULT_SO_REWARD; 
            conf_ptr->SO_MIN_TRANS_GEN_NSEC = DEFAULT_SO_MIN_TRANS_GEN_NSEC;
            conf_ptr->SO_MAX_TRANS_GEN_NSEC = DEFAULT_SO_MAX_TRANS_GEN_NSEC;
            conf_ptr->SO_RETRY = DEFAULT_SO_RETRY;			
            conf_ptr->SO_TP_SIZE = DEFAULT_SO_TP_SIZE;			
            conf_ptr->SO_MIN_TRANS_PROC_NSEC = DEFAULT_SO_MIN_TRANS_PROC_NSEC;
            conf_ptr->SO_MAX_TRANS_PROC_NSEC = DEFAULT_SO_MAX_TRANS_PROC_NSEC;
            conf_ptr->SO_SIM_SEC = DEFAULT_SO_SIM_SEC; 
            conf_ptr->SO_FRIENDS_NUM  = DEFAULT_SO_FRIENDS_NUM;		
            conf_ptr->SO_HOPS = DEFAULT_SO_HOPS;
        break;

//CONF1 grandissimo TP_SIZE, non verrano probabilmente creati nuovi nodi perchè difficilmente verranno 
//raggiunte le capienze delle pool
        case 1://------------------------------------------- inserimento automatico
            conf_ptr->SO_USERS_NUM = DEFAULT1_SO_USERS_NUM;		                
            conf_ptr->SO_NODES_NUM = DEFAULT1_SO_NODES_NUM;		
            conf_ptr->SO_BUDGET_INIT = DEFAULT1_SO_BUDGET_INIT;		
            conf_ptr->SO_REWARD = DEFAULT1_SO_REWARD; 
            conf_ptr->SO_MIN_TRANS_GEN_NSEC = DEFAULT1_SO_MIN_TRANS_GEN_NSEC;
            conf_ptr->SO_MAX_TRANS_GEN_NSEC = DEFAULT1_SO_MAX_TRANS_GEN_NSEC;
            conf_ptr->SO_RETRY = DEFAULT1_SO_RETRY;			
            conf_ptr->SO_TP_SIZE = DEFAULT1_SO_TP_SIZE;			
            conf_ptr->SO_MIN_TRANS_PROC_NSEC = DEFAULT1_SO_MIN_TRANS_PROC_NSEC;
            conf_ptr->SO_MAX_TRANS_PROC_NSEC = DEFAULT1_SO_MAX_TRANS_PROC_NSEC;
            conf_ptr->SO_SIM_SEC = DEFAULT1_SO_SIM_SEC; 
            conf_ptr->SO_FRIENDS_NUM  = DEFAULT1_SO_FRIENDS_NUM;		
            conf_ptr->SO_HOPS = DEFAULT1_SO_HOPS;	
        break;

//piccolissimio TP_SIZE, di solito vengono creati molti nuovi nodi per cercare di gestire le transizioni scartate
         case 2://------------------------------------------- inserimento automatico
            conf_ptr->SO_USERS_NUM = DEFAULT2_SO_USERS_NUM;		                
            conf_ptr->SO_NODES_NUM = DEFAULT2_SO_NODES_NUM;		
            conf_ptr->SO_BUDGET_INIT = DEFAULT2_SO_BUDGET_INIT;		
            conf_ptr->SO_REWARD = DEFAULT2_SO_REWARD; 
            conf_ptr->SO_MIN_TRANS_GEN_NSEC = DEFAULT2_SO_MIN_TRANS_GEN_NSEC;
            conf_ptr->SO_MAX_TRANS_GEN_NSEC = DEFAULT2_SO_MAX_TRANS_GEN_NSEC;
            conf_ptr->SO_RETRY = DEFAULT2_SO_RETRY;			
            conf_ptr->SO_TP_SIZE = DEFAULT2_SO_TP_SIZE;			
            conf_ptr->SO_MIN_TRANS_PROC_NSEC = DEFAULT2_SO_MIN_TRANS_PROC_NSEC;
            conf_ptr->SO_MAX_TRANS_PROC_NSEC = DEFAULT2_SO_MAX_TRANS_PROC_NSEC;
            conf_ptr->SO_SIM_SEC = DEFAULT2_SO_SIM_SEC; 
            conf_ptr->SO_FRIENDS_NUM  = DEFAULT2_SO_FRIENDS_NUM;		
            conf_ptr->SO_HOPS = DEFAULT2_SO_HOPS;	
        break;

//pochi users, valori più che ragionevoli e di solito arresto perche tutti gli utenti sono terminati
         case 3://------------------------------------------- inserimento automatico
            conf_ptr->SO_USERS_NUM = DEFAULT3_SO_USERS_NUM;		                
            conf_ptr->SO_NODES_NUM = DEFAULT3_SO_NODES_NUM;		
            conf_ptr->SO_BUDGET_INIT = DEFAULT3_SO_BUDGET_INIT;		
            conf_ptr->SO_REWARD = DEFAULT3_SO_REWARD; 
            conf_ptr->SO_MIN_TRANS_GEN_NSEC = DEFAULT3_SO_MIN_TRANS_GEN_NSEC;
            conf_ptr->SO_MAX_TRANS_GEN_NSEC = DEFAULT3_SO_MAX_TRANS_GEN_NSEC;
            conf_ptr->SO_RETRY = DEFAULT3_SO_RETRY;			
            conf_ptr->SO_TP_SIZE = DEFAULT3_SO_TP_SIZE;			
            conf_ptr->SO_MIN_TRANS_PROC_NSEC = DEFAULT3_SO_MIN_TRANS_PROC_NSEC;
            conf_ptr->SO_MAX_TRANS_PROC_NSEC = DEFAULT3_SO_MAX_TRANS_PROC_NSEC;
            conf_ptr->SO_SIM_SEC = DEFAULT3_SO_SIM_SEC; 
            conf_ptr->SO_FRIENDS_NUM  = DEFAULT3_SO_FRIENDS_NUM;		
            conf_ptr->SO_HOPS = DEFAULT3_SO_HOPS;	
        break;

        case 4://------------------------------------------- inserimento manuale
            printf("\n\nInserire i seguenti parametri:");
            printf("\nSO_USERS_NUM (default: %d): ",DEFAULT_SO_USERS_NUM);   scanf("%d",&conf_ptr->SO_USERS_NUM);
            printf("SO_NODES_NUM (default: %d): ",DEFAULT_SO_NODES_NUM);   scanf("%d",&conf_ptr->SO_NODES_NUM);
            printf("SO_BUDGET_INIT (default: %d): ",DEFAULT_SO_BUDGET_INIT);   scanf("%d",&conf_ptr->SO_BUDGET_INIT);
            printf("SO_REWARD (default: %d): ",DEFAULT_SO_REWARD);   scanf("%d",&conf_ptr->SO_REWARD);
            printf("SO_MIN_TRANS_GEN_NSEC (default: %d): ",DEFAULT_SO_MIN_TRANS_GEN_NSEC);   scanf("%d",&conf_ptr->SO_MIN_TRANS_GEN_NSEC);
            printf("SO_MAX_TRANS_GEN_NSEC (default: %d): ",DEFAULT_SO_MAX_TRANS_GEN_NSEC);   scanf("%d",&conf_ptr->SO_MAX_TRANS_GEN_NSEC);
            if(conf_ptr->SO_MIN_TRANS_GEN_NSEC>conf_ptr->SO_MAX_TRANS_GEN_NSEC)conf_ptr->SO_MAX_TRANS_GEN_NSEC=conf_ptr->SO_MIN_TRANS_GEN_NSEC;
            printf("SO_RETRY (default: %d): ",DEFAULT_SO_RETRY);   scanf("%d",&conf_ptr->SO_RETRY);
            printf("SO_TP_SIZE (default: %d): ",DEFAULT_SO_TP_SIZE);   scanf("%d",&conf_ptr->SO_TP_SIZE);
            printf("SO_MIN_TRANS_PROC_NSEC (default: %d): ",DEFAULT_SO_MIN_TRANS_PROC_NSEC);   scanf("%d",&conf_ptr->SO_MIN_TRANS_PROC_NSEC);
            printf("SO_MAX_TRANS_PROC_NSEC (default: %d): ",DEFAULT_SO_MAX_TRANS_PROC_NSEC);   scanf("%d",&conf_ptr->SO_MAX_TRANS_PROC_NSEC);
            if(conf_ptr->SO_MIN_TRANS_PROC_NSEC>conf_ptr->SO_MAX_TRANS_PROC_NSEC)conf_ptr->SO_MAX_TRANS_PROC_NSEC=conf_ptr->SO_MIN_TRANS_PROC_NSEC;
            printf("SO_SIM_SEC (default: %d): ",DEFAULT_SO_SIM_SEC);   scanf("%d",&conf_ptr->SO_SIM_SEC);
            printf("SO_FRIENDS_NUM (default: %d): ",DEFAULT_SO_FRIENDS_NUM);   scanf("%d",&conf_ptr->SO_FRIENDS_NUM);
            printf("SO_HOPS (default: %d): ",DEFAULT_SO_HOPS);   scanf("%d",&conf_ptr->SO_HOPS);      
        break;

        default:
            printf("\nERRORE di inserimento modalita (!= 0,1,2,3,4)\n");
            exit(EXIT_SUCCESS);
    }
    current_user=conf_ptr->SO_USERS_NUM;
}

void shm_get_at()
{    /* -------------------------- creazione un segmento di SHM atto a contenere libromastro ----------------------------*/                                                                     
    if ((shm_libro_id = shmget(KEY_SHM_LIBRO, sizeof(libromastro), IPC_CREAT | 0666)) == -1) //Returns shared memory segment identifier on success, or -1 on error
        errExit("[Master]: ho fallito nella shmget");
    //else    printf("[Master - PID = %ld]: HO CREATO il segmento di shm libro di id: %d\n", (long)getpid(), shm_libro_id);
    /* ------------------------------------------ aggancio del segmento -------------------------------------------------*/
    if ((libro_ptr = shmat(shm_libro_id, NULL, 0)) == (void *)-1)
        errExit("[Master]: ho fallito nella shmat");
    //else    printf("[Master - PID = %ld]: HO ATTACCATO il segmento di shm libro di id: %d al mio ptr\n", (long)getpid(), shm_libro_id);
    /* ----------------------------------------- inizializzo libromastro ------------------------------------------------*/
    libro_ptr->id_blocco_corrente = 0;

    /* ----------------------- creazione un segmento di SHM atto a contenere int numLettori ----------------------------*/                                                                     //shered memory id
    if ((shm_numLett_id = shmget(KEY_SHM_NUMLETT, sizeof(int), IPC_CREAT | 0666)) == -1) //Returns shared memory segment identifier on success, or -1 on error
        errExit("[Master]: ho fallito nella shmget");
    //else    printf("[Master - PID = %ld]: HO CREATO il segmento di shm numLett di id: %d\n", (long)getpid(), shm_numLett_id);
    /* ------------------------------------------ aggancio del segmento -------------------------------------------------*/
    if ((numLett_ptr = shmat(shm_numLett_id, NULL, 0)) == (void *)-1)
        errExit("[Master]: ho fallito nella shmat");
    //else    printf("[Master - PID = %ld]: HO ATTACCATO il segmento di shm numLett di id: %d al mio ptr\n", (long)getpid(), shm_numLett_id);
    /* ----------------------------------------- inizializzo numLett ------------------------------------------------*/
    *numLett_ptr = 0;

    /* ----------------------- creazione un segmento di SHM atto a contenere i processi creati ---------------------------*/
    if ((shm_listaUsers_id = shmget(KEY_SHM_PROC, sizeof(users_pid), IPC_CREAT | 0666)) == -1) 
        errExit("[Master]: ho fallito nella shmget l_proc");
    //else    printf("[Master - PID = %ld]: HO CREATO il segmento di shm proc di id: %d\n", (long)getpid(), shm_proc_id);
    /* ------------------------------------------ aggancio del segmento -------------------------------------------------*/
    if ((users_pid = shmat(shm_listaUsers_id, NULL, 0)) == (void *)-1)
        errExit("[Master]: ho fallito nella shmat");
    //else    printf("[Master - PID = %ld]: HO ATTACCATO il segmento di shm proc di id: %d al mio ptr\n", (long)getpid(), shm_proc_id);

     /* ----------------------- creazione un segmento di SHM atto a contenere i processi creati ---------------------------*/
    if ((shm_listaNodes_id = shmget(KEY_SHM_NODES, sizeof(nodes_pid), IPC_CREAT | 0666)) == -1) 
        errExit("[Master]: ho fallito nella shmget l_proc_nodes");
    //else    printf("[Master - PID = %ld]: HO CREATO il segmento di shm proc di id: %d\n", (long)getpid(), shm_proc_id);
    /* ------------------------------------------ aggancio del segmento -------------------------------------------------*/
    if ((nodes_pid = shmat(shm_listaNodes_id, NULL, 0)) == (void *)-1)
        errExit("[Master]: ho fallito nella shmat");
    //else    printf("[Master - PID = %ld]: HO ATTACCATO il segmento di shm proc di id: %d al mio ptr\n", (long)getpid(), shm_proc_id);

}

void setSEM()
{
    /* ----------------------------------------------- creo set semafori ------------------------------------------------*/
    if((sem_id = semget(KEY_SEM, 4, IPC_CREAT | 0600 ))==-1)
        errExit("[Master]: ho fallito nella semget");
    //else    printf("[Master - PID = %ld]: ho creato il semaforo di id= %d con successo!\n",(long) getpid(),sem_id);
    /* ------------------------------------ semafori di avvio concorrenza uteni-----------------------------------------*/       
    //------------------------inizializzo semaforo alla cardinalità della popolaz
    if(initSemWithValue(sem_id,SEM_PRONTI_TUTTI,(conf_ptr->SO_NODES_NUM+conf_ptr->SO_USERS_NUM))==-1)
        errExit("[Master]: ho fallito nella initSemWithValue SEM_PRONTI_TUTTI");
    //else    printf("[Master - PID = %ld]: ho inizializzato il semaforo SEM_PRONTI_TUTTI a %d\n",(long) getpid(),SO_NODES_NUM+SO_USERS_NUM);
    if(initSemWithValue(sem_id,SEM_LIBERI_TUTTI,1) == -1)
        errExit("[Master]: ho fallito nella initSemWithValue SEM_LIBERI_TUTTI");
    //else    printf("[Master - PID = %ld]: ho inizializzato il semaforo SEM_LIBERI_TUTTI a 1\n",(long) getpid());
   
    /*------------------------ MI RICONDUCO AL PROBLEMA DI LETTORI-SCRITTORI----------------------------------------------*/
    if(initSemWithValue(sem_id,SEM_MUTEX,1)==-1)//------------------user: aggironare numero lettori
        errExit("[Master]: ho fallito nella initSemWithValue");
    //else    printf("[Master - PID = %ld]: ho inizializzato il semaforo SEM_MUTEX a  %d\n",(long) getpid(),1);
    if(initSemWithValue(sem_id,SEM_SCRIVI,1)==-1)//-----------------node: avviare scrittura -> impedire altre scritture/letture
        errExit("[Master]: ho fallito nella initSemWithValue");
    //lse    printf("[Master - PID = %ld]: ho inizializzato il semaforo SEM_SCRIVI a  %d\n",(long) getpid(),1);

    /*sem_stampaVal(sem_id,SEM_LIBERI_TUTTI);
    sem_stampaVal(sem_id,SEM_MUTEX);
    sem_stampaVal(sem_id,SEM_SCRIVI);*/
}

void msgq()
{
    /* ------------------------------ creazione di una msg queue per scambio transizioni --------------------------------*/
    if ((msq_id = msgget(KEY_MSGQ, IPC_CREAT | 0644)) < 0) // | IPC_EXCL
        errExit("[Master]: ho fallito nella  msgget");
    //else    printf("[Master - PID = %ld]: HO CREATO la msg queue di id: %d\n", (long)getpid(), msq_id);

    /* ----------------------------- creazione di una msg queue per condivisione dei nodi --------------------------------*/
    if ((msq_nodi_p_id = msgget(KEY_MSGQ_NODI_PID, IPC_CREAT | 0644)) < 0) // | IPC_EXCL
        errExit("[Master]: ho fallito nella  msgget MSGQ_NODI_PID");

    /* ----------------------------- creazione di una msg queue per condivisione dei nodi --------------------------------*/
    if ((msq_trPoolCount_id = msgget(KEY_MSGQ_TRANS_POOL_COUNT, IPC_CREAT | 0644)) < 0) // | IPC_EXCL
        errExit("[Master]: ho fallito nella  msgget MSGQ_TRANS_POOL_COUNT");
}

/* ----------------------------------- creazione di SO_USERS_NUM processi utente ------------------------------------*/    
void creaUsers()
{
    char *u_argv[2] = {USER_NAME, NULL};                  //per convenzione argv[0] deve contenere il nome del file da eseguire
    int a; 
    for (int i = 0; i < conf_ptr->SO_USERS_NUM; i++)
    {
        switch (a = fork())
        {
        case -1:
            errExit("[Master]: ho fallito nella users fork()");
        case 0:           
            execve(USER_NAME, u_argv, NULL); 
			errExit("[user]: ho fallito nella execve()");//(passa di qui solo se execve fallisce)
            exit(EXIT_SUCCESS);
        default:
            users_pid[i]=a;
            //printf("[Master] ho aggiunto alla mia lista lo user: %ld\n",(long int)users_pid[i]);
            break;
        }
    }
}

 /* ------------------------------------ creazione di SO_NODES_NUM processi nodo -------------------------------------*/
void creaNodes()
{    
    char *n_argv[2] = {NODE_NAME, NULL}; 
    int a;                          
    for (int i = 0; i < conf_ptr->SO_NODES_NUM; i++)
    {
        switch ( a = fork())
        {
        case -1:
            errExit("[Master]: ho fallito nella nodes fork()");
        case 0:
            execve(n_argv[0], n_argv, NULL); 
			errExit("\n[node]: ho fallito nella execve()");//(passa di qui solo se execve fallisce)
            exit(EXIT_SUCCESS);
        default:
            nodes_pid[i]=a;
            if(i==0)
            {
                nodes_info = last_node = (node_info *)malloc(sizeof(node_info));
                last_node->pid=a;
                last_node->next=NULL;
            }        
            else{
            n = (node_info *)malloc(sizeof(node_info));
            n->pid=a;
            n->next = NULL;
            last_node->next=n;
            last_node = n;
            }
            //printf("[Master] ho aggiunto alla mia lista lo nodes: %d\n",last_node->pid);
        break;
        }
    }
}

/* ------------------------------------ invio dei pid nodo a utenti e nodi -------------------------------------*/
void mandaNodi()
{
    /*-------------------------------------------------- invio lista completa nodi a users
    for(int i=0; i<conf_ptr->SO_USERS_NUM;i++)
    {
        for(int j=0; j<conf_ptr->SO_NODES_NUM;j++)
        {
            msg_node_pid msg;
            msg.mtype=users_pid[i];
            msg.p=get_pid_node_info_index(nodes_info,j);
            //printf("\nmtype user dest: %ld,\t pid nodo: %d,\t i:%d, j:%d",msg.mtype,msg.p,i,j);
            if(msgsnd(msq_nodi_p_id, &msg, sizeof(msg)-sizeof(long),0)== -1) 
                errExit("[Master]: ho fallito nella msgsnd in mandaNodi()");
            
        }
    } */
    //---------------------------------------------------- invio SO_FRIENDS_NUM pid nodi a ogni nodo
    for(int i=0; i<conf_ptr->SO_NODES_NUM;i++)
    {
        int indici_casuali_scelti[conf_ptr->SO_FRIENDS_NUM];
        //--------------------- per ogni nodo ESTRAGGO A CASO SO_FRIENDS_NUM pid nodi
        for(int j=0; j<conf_ptr->SO_FRIENDS_NUM;j++)
        {//estraggo indici_casuali_scelti[j]
            int gia_estratto=1;
            while (gia_estratto)
            {
                gia_estratto=0;
                indici_casuali_scelti[j] = estrai_tra(0,conf_ptr->SO_NODES_NUM-1);
                //se un friend è il nodo ricevente stesso non va bene
                if(get_pid_node_info_index(nodes_info,indici_casuali_scelti[j]) == get_pid_node_info_index(nodes_info,i))
                    gia_estratto=1;
                else{
                    //se è uguale ad un amico già scelto non va bene
                    for(int z=0;z<j;z++)
                    {                        
                        if(indici_casuali_scelti[z] == indici_casuali_scelti[j])
                            gia_estratto=1;
                    }
                }
                
            }
        }
        //--------------------- invio elenco casuale di pid scelti al nodo
        for(int j=0; j<conf_ptr->SO_FRIENDS_NUM;j++)
        {
            msg_node_pid msg;
            msg.mtype=get_pid_node_info_index(nodes_info,i);
            msg.p=get_pid_node_info_index(nodes_info,indici_casuali_scelti[j]);
            if(msgsnd(msq_nodi_p_id, &msg, sizeof(msg)-sizeof(long),0)== -1) 
                errExit("[Master]: ho fallito nella msgsnd a nodi in mandaNodi()");
            //printf("\nmtype nodo dest: %ld,\t pid nodo inviato: %d,\t i:%d, indici_casuali_scelti[j]:%d",msg.mtype,msg.p,i,indici_casuali_scelti[j]);
        }
    }
}

/* ---------------------------------------- lettura mastro ------------------------------------------------------*/
void inizio_lettura()
{
    // Disattivo SIGCHLD
    sigset_t old_mask;
	old_mask = block_signals(3, SIGCHLD, SIGUSR1,SIGUSR2);

    if(reserveSem(sem_id,SEM_MUTEX)==-1) errExit("[Master]: ho fallito la reserve su SEM_MUTEX");
    //******************* SEZIONE CRITICA 
    *numLett_ptr+=1;
    if(*numLett_ptr==1)//se non sono il primo lettore, un user prima di me avrà già svolto questa operzione
    {//se sto leggendo il libro devo bloccare i nodi che vogliono scrivervi
        if(reserveSem(sem_id,SEM_SCRIVI)==-1) errExit("[Master]: ho fallito la reserve su SEM_SCRIVI");
    }
    //******************** END   
    if(releaseSem(sem_id,SEM_MUTEX)==-1) errExit("[Master]: ho fallito la release su SEM_MUTEX");

    reset_signals(old_mask);

}

void fine_lettura()
{
    // Disattivo SIGCHLD
    sigset_t old_mask;
	old_mask = block_signals(3, SIGCHLD, SIGUSR1,SIGUSR2);

    if(reserveSem(sem_id,SEM_MUTEX)==-1) errExit("[Master]: ho fallito la reserve su SEM_MUTEX");
    //******************* SEZIONE CRITICA 
    *numLett_ptr-=1;
    if(*numLett_ptr==0)//se non ci sono utenti che stanno leggendo il libro
    {//sblocco nodi che vogliono scrivervi
        if(releaseSem(sem_id,SEM_SCRIVI)==-1) errExit("[Master]: ho fallito la reserve su SEM_SCRIVI");
    }
    //******************** END   
    if(releaseSem(sem_id,SEM_MUTEX)==-1) errExit("[Master]: ho fallito la release su SEM_MUTEX");

    reset_signals(old_mask);
}

/* ------------------------------------ stampa a terminale del master  -------------------------------------*/
void stampa(int riepilogo)//se riepilogo è a 1 si tratta del riepilogo
{
    int pid_user_max, pid_user_min, pid_node_max, pid_node_min;
    int budjet_user_max, budjet_user_min, budjet_node_max, budjet_node_min;
    int stampa_tutti = (conf_ptr->SO_USERS_NUM<=MAX_VISUALIZ) ? 1 : 0;

    if(!riepilogo) printf("[Master]: Numero di processi utente attivi: %d\n",current_user);
    else printf("RIEPILOGO-BILANCI:\n");
    //---------------------------------stampa budjet utenti
    for(int z=0;z<conf_ptr->SO_USERS_NUM;z++)
    {
        int budj_corr_user=conf_ptr->SO_BUDGET_INIT;
        for(int i=0; i<libro_ptr->id_blocco_corrente; i++)
        {
            for(int j=0; j<SO_BLOCK_SIZE-1; j++)//ultima transaz di reward 
            {
                if(libro_ptr->vect_blocchi[i].vect_trans[j].receiver == users_pid[z])
                    budj_corr_user += libro_ptr->vect_blocchi[i].vect_trans[j].quantita;
                if(libro_ptr->vect_blocchi[i].vect_trans[j].sender == users_pid[z])
                    budj_corr_user -= libro_ptr->vect_blocchi[i].vect_trans[j].quantita + libro_ptr->vect_blocchi[i].vect_trans[j].reward;
            }
        }
        //----------------- gestisco stampa / calcolo dei pù significativi
        if(stampa_tutti) /*|| riepilogo*/
            printf("[User: %d]: budjet: %d \n",users_pid[z],budj_corr_user);
        else{
            if(z==0)//primo elemento
            {
                pid_user_max=users_pid[z];
                pid_user_min=users_pid[z];
                budjet_user_max=budj_corr_user;
                budjet_user_min=budj_corr_user;
            }
            else{//confronto
                if(budj_corr_user>budjet_user_max)
                {
                   pid_user_max = users_pid[z];
                   budjet_user_max = budj_corr_user;
                }
                if(budj_corr_user<budjet_user_min)
                {
                   pid_user_min = users_pid[z];
                   budjet_user_min = budj_corr_user;
                }
            }
        }
    }
    //---------------------------------stampa budjet nodi
    for(int z=0;z<l_nodes_info_count(nodes_info);z++)
    {
        int pid_curr_node = get_pid_node_info_index(nodes_info,z);
        int budj_corr_node=0;
        for(int i=0; i<libro_ptr->id_blocco_corrente; i++)
        {
            //ultima transaz di reward 
            if(libro_ptr->vect_blocchi[i].vect_trans[SO_BLOCK_SIZE-1].receiver == pid_curr_node){
                budj_corr_node+=libro_ptr->vect_blocchi[i].vect_trans[SO_BLOCK_SIZE-1].quantita;
            }
        }
        //----------------- gestisco stampa / calcolo dei pù significativi
        if(stampa_tutti || riepilogo)
        {   printf("[Node: %d]: budjet: %d",pid_curr_node,budj_corr_node);
            if(riepilogo) printf("\t-\ttransazioni in pool: %d",get_n_node_info_index(nodes_info,z));
            printf("\n");
        }
        else{
            if(z==0)//primo elemento
            {
                pid_node_max=pid_curr_node;
                pid_node_min=pid_curr_node;
                budjet_node_max=budj_corr_node;
                budjet_node_min=budj_corr_node;
            }
            else{//confronto
                if(budj_corr_node>budjet_node_max)
                {
                   pid_node_max = pid_curr_node;
                   budjet_node_max = budj_corr_node;
                }
                if(budj_corr_node<budjet_node_min)
                {
                   pid_node_min = pid_curr_node;
                   budjet_node_min = budj_corr_node;
                }
            }
        }
    }
    //---------------------------------stampa solo più significativi
    if(!(stampa_tutti || riepilogo))
    {
        printf("[Node: %d]: ho il budjet minore: %d\n",pid_node_min,budjet_node_min);
        printf("[Node: %d]: ho il budjet maggiore: %d\n",pid_node_max,budjet_node_max);
    }  
    if(!(stampa_tutti))
    {
        printf("[User: %d]: ho il budjet minore: %d\n",pid_user_min,budjet_user_min);
        printf("[User: %d]: ho il budjet maggiore: %d\n",pid_user_max,budjet_user_max);
    }        
    //---------------------------------stampa riepilogo
    if(riepilogo)
    {
        printf("\n---> ---> ----> [Master]: Numero di processi utente terminati prematuramente: %d\n",conf_ptr->SO_USERS_NUM - current_user);
        printf("---> ---> ----> [Master]: Numero di blocchi nel libro mastro: %d\n",libro_ptr->id_blocco_corrente);
        printf("---> ---> ----> [Master]: Numero di processi nodo generati per gestire le transazioni scartate: %d\n",l_nodes_info_count(nodes_info)-conf_ptr->SO_NODES_NUM);
        printf("---> ---> ----> [Master]: Numero di transizioni ancora presenti in transaction pool STAMPATE A FIANCO DEI BILANCI\n\n");
    }
}

/* ------------------------------------ hendler ricezione segnali --------------------------------------------------*/
void sigchld_handler(int signum){//uno user è terminato
    if(!blocca_current_user)current_user--;//aggiorno numero di processi utente attivi
}

void sigalrm_handler(int signum){
    arresto(TEMPO_SCADUTO);
}

void sigurs1_hendler(int signum){//seganle da un nodo che ha raggiunto capienza massima libro
    arresto(FULL_LIBRO);
}

void sigurs2_hendler(int signum){//seganle da un nodo che mi ha inviato una transazione
    //----------------------- controllo ricezione di transazione da parte di un nodo
    msg_trans new_trans;
    if((msgrcv(msq_id, &new_trans, (sizeof(new_trans)-(sizeof(long))), getpid(),0))== -1)
        errExit("[Master] msgrcv error");
    else
        got_trans(new_trans);  
}

/* ------------------------- arresto dovuto alle 3 possibili ragioni di terminazione ------------------------------------*/
void arresto(int motivo){
    sigset_t old_mask;
    blocca_current_user=1;
    //---------------------------termino utenti
	for(int i = 0; i < conf_ptr->SO_USERS_NUM; i++) {
		//printf("[Master - PID = %ld]: termino user: %d\n",(long)getpid(), l_proc_ptr->users_pids[i]);
		//if(kill(l_proc_ptr->users_pids[i], 0))
        kill(users_pid[i], SIGTERM);
	}
    //---------------------------termino nodi
    for(int i = 0; i < l_nodes_info_count(nodes_info); i++) {
    	//printf("[Master - PID = %ld]: termino node: %d\n",(long)getpid(), l_proc_ptr->nodes_pids[i]);
		kill(get_pid_node_info_index(nodes_info,i), SIGTERM);
	}
	while (wait(NULL) != -1);
    //---------------------------terminati i nodi posso mettermi in ricezione del loro messggio contentene i nodi in tr_pool
    for(int i=1;i<l_nodes_info_count(nodes_info);i++)
    {
        msg_trans_pool_count msg;
        if((msgrcv(msq_trPoolCount_id, &msg, sizeof(msg_trans_pool_count)-(sizeof(long)), getpid(),0))== -1)
            errExit("[Master] msgrcv error");
        lista_nodi_info l = nodes_info;
        while(l!=NULL)//ricerco nodo corretto per pid e gli assegno n° transizioni nella sua pool
        {
            if(l->pid==msg.node_pid)
                l->n=msg.tr_pool_count;
            l=l->next;
        }
    }
    printf("\n---> ---> ----> [Master - PID = %ld]: Arresto la simulazione!\n",(long)getpid());
    switch (motivo){
        case TEMPO_SCADUTO:
            printf("---> ---> ----> [Master - PID = %ld]: Ragione arresto: Tempo scaduto\n\n",(long)getpid());
            break;
        case ALL_USERS_TERMINATED:
            printf("---> ---> ----> [Master - PID = %ld]: Ragione arresto: Tutti gli utenti sono terminati\n\n",(long)getpid());
            break;
        case FULL_LIBRO:
            printf("---> ---> ----> [Master - PID = %ld]: Ragione arresto: Capienza libro mastro esaurita\n\n",(long)getpid());
        break;
    }        

    stampa(STAMPA_FINALE);

    //---------------------------dealloco risorse
    if(shmctl(shm_libro_id, IPC_RMID, NULL) == -1) errExit("[Master]: ho fallito la shmdt libro");
    if(shmctl(shm_numLett_id, IPC_RMID, NULL) == -1) errExit("[Master]: ho fallito la shmdt numLett");
    if(shmctl(shm_listaUsers_id, IPC_RMID, NULL) == -1) errExit("[Master]: ho fallito la shmdt l_proc_users");
    //if(shmctl(shm_listaNodes_id, IPC_RMID, NULL) == -1) errExit("[Master]: ho fallito la shmdt l nodes");
    if(semctl(sem_id, 0, IPC_RMID)) errExit("[Master]: ho fallito la IPC_RMIND sem");
    if(msgctl(msq_id, IPC_RMID, NULL)) errExit("[Master]: ho fallito la IPC_RMIND msg queue");
    if(msgctl(msq_nodi_p_id, IPC_RMID, NULL)) errExit("[Master]: ho fallito la IPC_RMIND msg queue");
    if(msgctl(msq_trPoolCount_id, IPC_RMID, NULL)) errExit("[Master]: ho fallito la IPC_RMIND msg queue");
    free(nodes_info);

    exit(EXIT_SUCCESS);//bye
}

/* ----------------------- ricevo transazione da un nodo (scartata SO_HOPS_NUM volte) ---------------------------------*/
void got_trans(msg_trans new_trans)
{   //--------------------------incremento il sem a 1 per l'avvio del nuovo nodo
    if(releaseSem(sem_id,SEM_PRONTI_TUTTI)==-1) errExit("[Node]: ho fallito la reserveSem 0"); 

    //--------------------------------------- creo nuovo nodo
    char *n_argv[2] = {NODE_NAME, NULL}; 
    int a;                          
    switch ( a = fork())
    {
        case -1:
            errExit("[Master]: ho fallito nella nodes fork() in got_trans()");
        case 0:
            execve(n_argv[0], n_argv, NULL); 
			    errExit("\n[node]: ho fallito nella execve()");//(passa di qui solo se execve fallisce)
        default:     
            n = (node_info *)malloc(sizeof(node_info));
            n->pid=a;
            n->next = NULL;
            last_node->next=n;
            last_node = n;
            //printf("[Master] ho aggiunto alla mia lista lo nodes: %d\n",last_node->pid);
        break;
    }
    //--------------------------------------- gli invio la transazione che sarà la prima in lista
    new_trans.hops=conf_ptr->SO_HOPS;
    new_trans.mtype = (long)a;
    sigset_t old_mask;
	//old_mask = block_signals(3, SIGCHLD, SIGUSR1,SIGUSR2);
    if(msgsnd(msq_id, &new_trans, sizeof(new_trans)-sizeof(long), IPC_NOWAIT)== -1)
        errExit("[Master]: ho fallito nella msgsnd trans al nuovo nodo");
    //else kill(a,SIGUSR1); //------------------------ informo nodo della ricezione
    //reset_signals(old_mask);

    //--------------------------------------- invio elenco casuale di pid scelti come friends al nodo appena creato
    int *casuali = estrai_aCaso_tranne(a,conf_ptr->SO_FRIENDS_NUM);// ESTRAGGO A CASO 
    //--------------------- gli invio SO_FRIENDS_NUM pid nodi
    for(int j=0; j<conf_ptr->SO_FRIENDS_NUM;j++)
    {
        msg_node_pid msg;
        msg.mtype=a;
        msg.p=get_pid_node_info_index(nodes_info,casuali[j]);
        //old_mask = block_signals(3, SIGCHLD, SIGUSR1,SIGUSR2);
        if(msgsnd(msq_nodi_p_id, &msg, sizeof(msg)-sizeof(long),0)== -1) 
            errExit("[Master]: ho fallito nella msgsnd a nodi in got_trans()");
        //reset_signals(old_mask);
    }   

    //---------------------------------------- estraggo caso altri SO_NUM_FRIENDS processi nodo già esistenti
    casuali = estrai_aCaso_tranne(a,conf_ptr->SO_FRIENDS_NUM);
    //---------------------------------------- ordinandogli di aggiungere alla lista dei loro amici il nodo appena creato
    for(int j=0; j<conf_ptr->SO_FRIENDS_NUM;j++)
    {
        msg_node_pid msg;
        msg.mtype=get_pid_node_info_index(nodes_info,casuali[j]);
        msg.p=a;
        //old_mask = block_signals(3, SIGCHLD, SIGUSR1,SIGUSR2);
        if(msgsnd(msq_nodi_p_id, &msg, sizeof(msg)-sizeof(long),0)== -1) 
            errExit("[Master]: ho fallito nella msgsnd a nodi in got_trans()");
        else kill(msg.mtype,SIGUSR2); //------------------------ informo nodo della ricezione
        //reset_signals(old_mask);
    }   


}

int* estrai_aCaso_tranne(int pid, int dime)
{
    int *indici_casuali_scelti=calloc(dime,sizeof(int));
    
    for(int j=0; j<dime;j++)
    {//estraggo indici_casuali_scelti[j]
        int gia_estratto=1;
        while (gia_estratto)
        {
            gia_estratto=0;
            indici_casuali_scelti[j] = estrai_tra(0,l_nodes_info_count(nodes_info)-1);
            //se un friend è il nodo ricevente stesso non va bene
            if(get_pid_node_info_index(nodes_info,indici_casuali_scelti[j]) ==pid)
                gia_estratto=1;
            else{
                //se è uguale ad un amico già scelto non va bene
                for(int z=0;z<j;z++)
                {                        
                    if(indici_casuali_scelti[z] == indici_casuali_scelti[j])
                        gia_estratto=1;
                }
            }
        }
    }
    return indici_casuali_scelti;
}