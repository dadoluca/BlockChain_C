#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <signal.h>

#define USER_NAME "./user"
#define NODE_NAME "./node"
#define KEY_MSGQ 33333333//trans
#define KEY_MSGQ_NODI_PID 44444444//invio pid nodi
#define KEY_MSGQ_TRANS_POOL_COUNT 55555555
#define KEY_SHM_LIBRO 22222222
#define KEY_SHM_NUMLETT 3333
#define KEY_SHM_PROC 88888888
#define KEY_SHM_NODES 77777777
#define KEY_SHM_CONF 66666666
#define KEY_SEM 55556
#define SEM_LIBERI_TUTTI 0
#define SEM_MUTEX 1
#define SEM_SCRIVI 2
#define SEM_PRONTI_TUTTI 3
#define REWARD_SENDER_VAL -1
#define MAX_VISUALIZ 10 //se ci sono più di MAX_VISUALIZ user verranno stampate solo le informazioni relative ai processi piu signific
//motivi di arresto simulazione:
#define TEMPO_SCADUTO 0
#define ALL_USERS_TERMINATED 1
#define FULL_LIBRO 2

#define STAMPA_PERIODICA 0
#define STAMPA_FINALE 1


#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); \
                               } while (0)
                                    //      conf3  conf2   conf1  myConf
#define SO_BLOCK_SIZE					    /*10   */10   /*100   10*/
#define SO_REGISTRY_SIZE				    /*1000 */10000/*1000  1000*/        //mettendo 20 --> raggiungo subito capienza e verifico msg arresto

/* ---------------------------------------- definisco struct conf ----------------------------------------*/
    typedef struct { 
        int SO_USERS_NUM;					  
        int SO_NODES_NUM;					  
        int SO_BUDGET_INIT;					  
        int SO_REWARD; //[0–100]			
        int SO_MIN_TRANS_GEN_NSEC; //[nsec]
        int SO_MAX_TRANS_GEN_NSEC; //[nsec]
        int SO_RETRY;					      
        int SO_TP_SIZE;					      
        int SO_MIN_TRANS_PROC_NSEC; //[nsec]
        int SO_MAX_TRANS_PROC_NSEC; //[nsec]
        int SO_SIM_SEC; //[sec]
        int SO_FRIENDS_NUM;					  
        int SO_HOPS;	
    }conf;

/* ---------------------------------------- definisco struct libromastro ----------------------------------------*/
    typedef struct { 
        time_t timestamp;             //clock_gettime(...)
        int sender;                   //utente che ha generato la transazione
        int receiver;                 //utente destinatario della somma
        int quantita;                 //quantità di denaro inviata
        int reward;                   //denaro pagato dal sender al NODO che processa la transazione
    }transizione;
    
    typedef struct { 
        int id;
        transizione vect_trans[SO_BLOCK_SIZE];
    }blocco;
    
    typedef struct { 
        blocco vect_blocchi[SO_REGISTRY_SIZE];
        int id_blocco_corrente;//da tenere?  //mi permette di sapere quanti blocchi ho già inserito
    }libromastro;

    typedef struct nod{//da user,node
        pid_t pid;
        struct nod *next;
    }nodo;
    typedef nodo *lista_nodi;

    typedef struct _nodo//da master per il momento
    {
        pid_t pid;
        int n;//nodi nella transaction pool a fine esecuzione
        struct _nodo *next;
    }node_info;
    typedef node_info *lista_nodi_info;


 /* ---------------------------------------- definisco struct msg  transizione ------------------------------------------*/
    typedef struct {   long mtype;    transizione t; int hops;  } msg_trans;
 /* ---------------------------------------- definisco struct msg  node pid    ------------------------------------------*/
    typedef struct {   long mtype;    pid_t p;   } msg_node_pid;
/* -------------------------------------- definisco struct msg  trans_pool count ----------------------------------------*/
    typedef struct {   long mtype;    pid_t node_pid;   int tr_pool_count;   } msg_trans_pool_count;



//SEGUE CODICE SEMAFORI VISTO A LEZIONE ! *********************************************************
/* ------------------------------ INITIALIZE SEMAPHORE TO 0 ("in use") ---------------------------------------------------*/
int initSemWithValue(int semId, int semNum, int semVal);

/* ------------------------------ RESERVE SEMAPHORE - DECREMENT IT BY 1 --------------------------------------------------*/
int reserveSem(int semId, int semNum);

/* ------------------------------ RELEASE SEMAPHORE - INCREMENT IT BY 1 --------------------------------------------------*/
int releaseSem(int semId, int semNum);
//**************************************************************************************************


/* ----------------------------------------- EXECUTE A SEMOP -------------------------------------------------------------*/
int semOp(int s_id, unsigned short sem_num, short sem_op, short sem_flg);

/* ----------------------------------------- stampa valore sem -----------------------------------------------------------*/
void sem_stampaVal(int s_id, unsigned short sem_num);//funzione usata principalmente per testare il programma

/* ----------------------------------------- valore casuale tra -----------------------------------------------------------*/
long estrai_tra(long min,long max);
/* ----------------------------------------- conta nodi in lista ----------------------------------------------------------*/
int l_nodes_count(lista_nodi l);
/* ---------------------------- restituisce il pid del nodo index della lista ---------------------------------------------*/
int get_pid_node_index(lista_nodi l,int i);
/* ----------------------------------------- conta nodi in lista ----------------------------------------------------------*/
int l_nodes_info_count(lista_nodi_info l);
/* ---------------------------- restituisce il pid del nodo index della lista ---------------------------------------------*/
int get_pid_node_info_index(lista_nodi_info l,int i);
/* --------- restituisce il campo n(numero di nodi in pool a termine simulazione) del nodo index della lista ---------------------------------*/
int get_n_node_info_index(lista_nodi_info l,int i);




//SEGUE CODICE MASK SIGNALS DA MOODLE: Esempio progetto anni passati! *******************************
/* Blocca i segnali elencati tra gli argomenti */
sigset_t block_signals(int count, ...);/* Restituisce la vecchia maschera */

/* Sblocca i segnali elencati tra gli argomenti */
sigset_t unblock_signals(int count, ...);/* Restituisce la vecchia maschera */

/* Imposta una maschera per i segnali, usata per reimpostare una vecchia maschera ritornata da block_signals*/ 
void reset_signals(sigset_t old_mask);
//**************************************************************************************************



#endif



                        



