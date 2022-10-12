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
#include <signal.h>
#include <errno.h>

typedef struct _node {
    transizione *t;
    struct _node *next;
} node; // Definizione del tipo 'node' usato per rappresentare i nodi della lista
typedef node *list_t;

//-------------------------------- var globali
conf *conf_ptr;
libromastro *libro_ptr;
lista_nodi friends=NULL;//nodi amici
lista_nodi last_friend;
nodo *n;
list_t transaction_pool=NULL;//first node to process
list_t last;//last node to process
node *e;
blocco new_blocco;
int shm_conf_id, shm_libro_id, msq_id, sem_id,msq_trPoolCount_id,msq_nodi_p_id; 
int tp_count=0;

void my_IPC_access();
void got_trans(msg_trans new_trans);
blocco make_blocco();
void write_blocco();
void dimensione_raggiunta();
void l_print(list_t l);
void l_print_(lista_nodi l);
int l_count(list_t l);
void sigterm_handler(int signum);
//void sigurs1_hendler(int signum);
void sigurs2_hendler(int signum);

int main(int argc, char *argv[]){
    //printf("[Node: %ld]\n",(long)getpid());
    //--------------------------recupero l'accesso agli oggetti IPC
    my_IPC_access();
    //--------------------------decremento il sem SEM_PRONTI_TUTTI
    if(reserveSem(sem_id,SEM_PRONTI_TUTTI)==-1) errExit("[Node]: ho fallito la reserveSem SEM_PRONTI_TUTTI"); 
    /*else printf("[Node - PID = %ld]: ho decrementato il semaforo 0\n",(long) getpid());*/

    //------------------------------ ricezione segnale di terminazione da parte del padre
    if(signal(SIGTERM,sigterm_handler)==SIG_ERR) errExit("[Node]: ho fallito nella signal SIGTERM");
    //------------------------------ ricezione segnale di arrivo transazione da parte di un user/nodo/master
    //if(signal(SIGUSR1,sigurs1_hendler)==SIG_ERR) errExit("[Node]: ho fallito nella signal SIGUSR1");
    //------------------------------ ricezione segnale di arrivo nuovo freind crato dal master per trans scartata
     if(signal(SIGUSR2,sigurs2_hendler)==SIG_ERR) errExit("[Node]: ho fallito nella signal SIGUSR2");
    //--------------------------riempo lista nodi friends!
    for(int i=0;i<conf_ptr->SO_FRIENDS_NUM;i++)
    {
      msg_node_pid msg;
        if((msgrcv(msq_nodi_p_id, &msg, sizeof(msg_node_pid)-(sizeof(long)), getpid(),0))== -1)
            errExit("[User] msgrcv error");
        if(i==0)
        {
          friends = last_friend = (nodo *)malloc(sizeof(nodo));
          last_friend->pid=msg.p;
          last_friend->next=NULL;
        }        
        else{
          n = (nodo *)malloc(sizeof(nodo));
          n->pid=msg.p;
          n->next = NULL;
          last_friend->next=n;
          last_friend = n;
        }
        //printf("\n[Node: %d] i:%d, pid:%d\n",getpid(),i,(int)msg.p);
    }
    //l_print_(friends);
    //--------------------------------------------------------- mi metto in ascolto di messaggi
    while (1) {
      /*--------------------------ricevo nodo friend (CREATO DAL MASTER per transaz scartata) da aggiungere?
      msg_node_pid msg;
      if((msgrcv(msq_nodi_p_id, &msg, sizeof(msg_node_pid)-(sizeof(long)), getpid(),0))== -1)
            errExit("[Node] msgrcv error");
        else{
          n = (nodo *)malloc(sizeof(nodo));
          n->pid=msg.p;
          n->next = NULL;
          last_friend->next=n;
          last_friend = n;
        }      printf("\nciao");*/
      //--------------------------ricevo transazione?
      msg_trans new_trans;
      if((msgrcv(msq_id, &new_trans, (sizeof(new_trans)-(sizeof(long))), getpid(),0))== -1)
      {  if(errno != EINTR) //interrotta non dall'hendeler di un segnale di fine simulazione SIGTERM
            errExit("[Node] msgrcv trans error");
      }
      else
        //printf("[Node - PID = %ld]: Ho ricevuto una transazione da: %ld\n",(long)getpid(),(long)new_trans.t.sender);
        got_trans(new_trans);    
    }
    l_print(transaction_pool);

    exit(EXIT_SUCCESS);
}

/* ------------------------------------ recupero l'accesso agli oggetti IPC-------------------------------------------*/
void my_IPC_access(){
    if((msq_id = msgget(KEY_MSGQ, 0600)) == -1) errExit("[Node]: ho fallito la msgget");
    if((msq_trPoolCount_id = msgget(KEY_MSGQ_TRANS_POOL_COUNT, 0600)) == -1) errExit("[Node]: ho fallito la msgget");
    if((msq_nodi_p_id = msgget(KEY_MSGQ_NODI_PID, 0600)) == -1) errExit("[User]: ho fallito la msgget nodi_p");
	  if((sem_id = semget(KEY_SEM, 0, 0600)) == -1) errExit("[Node]: ho fallito la semget");
    if((shm_conf_id = shmget(KEY_SHM_CONF, 0, 0666)) == -1) errExit("[Node]: ho fallito la shmget conf");    
    if((conf_ptr = (conf *)shmat(shm_conf_id,  NULL, 0)) == (void *)-1) errExit("[Node]: ho fallito nella shmat conf_ptr");
    if((shm_libro_id = shmget(KEY_SHM_LIBRO, sizeof(libromastro), 0600)) == -1) errExit("[Node]: ho fallito la shmget libro"); 
    if((libro_ptr = (libromastro *)shmat(shm_libro_id,NULL,0))==(void *)-1) errExit("[Node]: ho fallito nella shmat libroMastro");       
}

/* ------------------------------------ avvenuta ricezione di una transaz.-------------------------------------------*/
void got_trans(msg_trans new_trans)
{ //--------------------------raggiunto numero massimo di transazioni
  if(tp_count==conf_ptr->SO_TP_SIZE)//conf_ptr->SO_TP_SIZE)//lista piena /*x==conf_ptr->SO_TP_SIZE*/
  {//printf("piena");
    //-------------- se la transazione ha finito gli hops diponibili la mando al master
    if(new_trans.hops==0)
    {
      new_trans.hops=conf_ptr->SO_HOPS;
      new_trans.mtype=getppid();
      if(msgsnd(msq_id, &new_trans, sizeof(new_trans)-sizeof(long), IPC_NOWAIT)== -1)
        errExit("[Node]: ho fallito nella msgsnd al master");
      //-----------------------------avverto il master con un segnale SIGUSR2!
      else kill(getppid(),SIGUSR2);
      //printf("[Node : %d] Lista piena! Ho spedito la transazione al master!\n",getpid());
    }
    //-------------- altrimenti la mando ad un nodo friend scelto a caso
    else
    {
      new_trans.hops--;
      int randomNode = estrai_tra(0,l_nodes_count(friends)-1);
      new_trans.mtype = (long)get_pid_node_index(friends,randomNode);
      /*if(msgsnd(msq_id, &new_trans, sizeof(new_trans)-sizeof(long), IPC_NOWAIT)== -1)
      {
        int fallimenti_consec=1;
        while(msgsnd(msq_id, &new_trans, sizeof(new_trans)-sizeof(long), IPC_NOWAIT)==-1 && fallimenti_consec<conf_ptr->SO_RETRY)
        {   
            fallimenti_consec++;
        }
      }*/
      if(msgsnd(msq_id, &new_trans, sizeof(new_trans)-sizeof(long), IPC_NOWAIT)== -1)
        errExit("[Node]: ho fallito nella msgsnd a un nodo friend");
      //else kill(new_trans.mtype,SIGUSR1);//--------------------------AVVISO NODO RICEVITORE CON UN SEGNALE SIGURS1
    }
  }
  else{
    int x;
    switch (x=l_count(transaction_pool))/*x=l_count(transaction_pool)*/
    {
      //--------------------------nodo testa
      case 0:
        //--------------------------alloco
        transaction_pool = last = (node *)malloc(sizeof(node));
        last->t=malloc(sizeof(transizione));
        last->t->timestamp=new_trans.t.timestamp;      last->t->quantita = new_trans.t.quantita;      last->t->sender = new_trans.t.sender;
        last->t->receiver = new_trans.t.receiver;      last->t->reward = new_trans.t.reward;

        //printf("[Node - PID = %ld]: \t\t\t\t\t\tcreo transaz, sender: %d\n",(long)getpid(),transaction_pool->t->sender);
        last->next = NULL; 
        tp_count++;

        if(SO_BLOCK_SIZE==2)//transizione testa + quella di reward
          dimensione_raggiunta();     
      break;
      
      default:
        //--------------------------aggiungo transizione alla lista
        e = (node *)malloc(sizeof(node));
        e->t = malloc(sizeof(transizione));
        e->t->timestamp=new_trans.t.timestamp;      e->t->quantita = new_trans.t.quantita;      e->t->sender = new_trans.t.sender;
        e->t->receiver = new_trans.t.receiver;      e->t->reward = new_trans.t.reward;
        e->next = NULL;
        last->next=e;
        last = e;
        //printf("[Node - PID = %ld]: \t\t\t\t\t\tcreo transaz sender: %d - prec nodesender: %d\n",(long)getpid(),last->t->sender,transaction_pool->t->sender);
        tp_count++;
        //--------------------------controllo se ho raggiunto dimensione blocco
        //if(x+1==SO_BLOCK_SIZE-1)//+1 per la trans appena aggiunta, -1 per la transaz reward 
        if(x+1==SO_BLOCK_SIZE-1)
          {dimensione_raggiunta();}
      break;
    }
  }
  //l_print(transaction_pool);      
}

void dimensione_raggiunta()
{
  //--------------------------creo blocco candidato
  new_blocco = make_blocco();
  //printf("[Node - PID = %ld]: Ho creato un blocco\n",(long)getpid());
  //--------------------------attesa inattiva
  struct timespec tim, tim2;
  tim.tv_sec = 0;  tim.tv_nsec = estrai_tra(conf_ptr->SO_MIN_TRANS_PROC_NSEC,conf_ptr->SO_MAX_TRANS_PROC_NSEC);
  nanosleep(&tim,&tim2);
  //new_blocco = make_blocco();
  //printf("[Node - PID = %ld]: Tempo elaborazione blocco terminato (%ld nsec), mi appresto a scrivere!\n",(long)getpid(),(long)tim.tv_nsec);
  //--------------------------scrivo blocco candidato nel libro
  write_blocco(new_blocco);
  //printf("[Node - PID = %ld]: Ho scritto il blocco nel libro mastro\n",(long)getpid());
}

/* ------------------------------------ creazione del blocco candidato -------------------------------------------*/
blocco make_blocco()
{
  blocco blocco_candidato;
  //list_t trans = transaction_pool;
  int reward_sum=0;
  //--------------------------copio LE PRIME SO_BLOCK_SIZE-1 transaz dalla transaction_pool
  for(int i=0;i<SO_BLOCK_SIZE-1;i++)
  {
    blocco_candidato.vect_trans[i].timestamp = transaction_pool->t->timestamp;
    blocco_candidato.vect_trans[i].sender = transaction_pool->t->sender;
    blocco_candidato.vect_trans[i].receiver = transaction_pool->t->receiver;
    blocco_candidato.vect_trans[i].quantita = transaction_pool->t->quantita;
    blocco_candidato.vect_trans[i].reward = transaction_pool->t->reward;

    reward_sum += blocco_candidato.vect_trans[i].reward;
    transaction_pool=transaction_pool->next;
  }
  //--------------------------aggiungo transazione reward
  //-------------prelevo il timestamp in nanosec
  struct timespec t;
  if(clock_gettime(CLOCK_REALTIME, &t)==-1) errExit("[Node]: ho fallito nella clock_gettime in make_blocco()");
  blocco_candidato.vect_trans[SO_BLOCK_SIZE-1].timestamp = t.tv_nsec;
  blocco_candidato.vect_trans[SO_BLOCK_SIZE-1].sender = REWARD_SENDER_VAL;
  blocco_candidato.vect_trans[SO_BLOCK_SIZE-1].receiver = getpid();
  blocco_candidato.vect_trans[SO_BLOCK_SIZE-1].quantita = reward_sum;
  blocco_candidato.vect_trans[SO_BLOCK_SIZE-1].reward = 0;

  return blocco_candidato;
}

/* ------------------------------------ scrittura blocco nel libro -------------------------------------------*/
void write_blocco(blocco blocco_candidato)
{
  // Disattivo segnali
  sigset_t old_mask;
	old_mask = block_signals(3, SIGTERM, SIGUSR1, SIGUSR2);

  if(reserveSem(sem_id,SEM_SCRIVI)==-1) errExit("[Node]: ho fallito la reserve su SEM_SCRIVI");
  //******************* SEZIONE CRITICA 
  //printf("[Node - PID = %ld]: \t\t\tHO ACQUISITO SCRIVI\n",(long)getpid());
  blocco_candidato.id = libro_ptr->id_blocco_corrente;
  libro_ptr->vect_blocchi[libro_ptr->id_blocco_corrente] = blocco_candidato;
  libro_ptr->id_blocco_corrente++;
  //******************** END   
  if(releaseSem(sem_id,SEM_SCRIVI)==-1) errExit("[Node]: ho fallito la release su SEM_SCRIVI");
  //printf("[Node - PID = %ld]: \t\t\tHO RILASCIATO SCRIVI\n",(long)getpid());

  if(libro_ptr->id_blocco_corrente == SO_REGISTRY_SIZE)
  {//devo bloccare scritture e inviare segnale al master che termini la simulazione
    if(reserveSem(sem_id,SEM_SCRIVI)==-1) errExit("[Node]: ho fallito la reserve su SEM_SCRIVI");
    kill(getppid(),SIGUSR1);
  }

  reset_signals(old_mask);
}

void l_print(list_t l){
	printf("[Node - PID = %ld]: (",(long)getpid());
	while(l!=NULL) {   //sintassi for che utilizza variabile gia istanziata (parametro)
	   printf("%d, ",l->t->sender);
     l=l->next;}
	printf("\b\b)\n");
}


void l_print_(lista_nodi l){
	printf("[Node - PID = %ld]: (",(long)getpid());
	while(l!=NULL) {   //sintassi for che utilizza variabile gia istanziata (parametro)
	   printf("%d, ",l->pid);
     l=l->next;}
	printf("\b\b)\n");
}

int l_count(list_t l)
{
  int count=0;
  while(l!=NULL) {   //sintassi for che utilizza variabile gia istanziata (parametro)
	  count++;
    l=l->next;
  }
  return count;
}

/* ------------------------------------ termino e mando messaggio master -------------------------------------------*/
void sigterm_handler(int signum)
{
  msg_trans_pool_count msg;
  msg.mtype=getppid();
  msg.node_pid=getpid();
  msg.tr_pool_count=l_count(transaction_pool);
  //printf("\nmtype user dest: %ld,\t pid nodo: %d,\t i:%d, j:%d",msg.mtype,msg.p,i,j);
  if(msgsnd(msq_trPoolCount_id, &msg, sizeof(msg)-sizeof(long), IPC_NOWAIT)== -1) 
    errExit("[Node]: ho fallito nella msgsnd in sigterm_handler(SIGTERM)");
  exit(EXIT_SUCCESS);
}

/* ------------------- ricezione segnale di di arrivo transazione da parte di un user/nodo/master --------------------*/
/*void sigurs1_hendler(int signum){
//--------------------------ricevo transazione?
  msg_trans new_trans;
  if((msgrcv(msq_id, &new_trans, (sizeof(new_trans)-(sizeof(long))), getpid(),0))== -1)
  {  if(errno != EINTR) //interrotta non dall'hendeler di un segnale di fine simulazione SIGTERM
        errExit("[Node] msgrcv error");
  }
  else
    //printf("[Node - PID = %ld]: Ho ricevuto una transazione da: %ld\n",(long)getpid(),(long)new_trans.t.sender);
    got_trans(new_trans);
}*/

/* ------------------- ricevo nodo friend (CREATO DAL MASTER per transaz scartata) da aggiungere --------------------*/
void sigurs2_hendler(int signum){
  msg_node_pid msg;
  if((msgrcv(msq_nodi_p_id, &msg, sizeof(msg_node_pid)-(sizeof(long)), getpid(),0))== -1)
        errExit("[Node] msgrcv error");
  else{
    n = (nodo *)malloc(sizeof(nodo));
    n->pid=msg.p;
    n->next = NULL;
    last_friend->next=n;
    last_friend = n;

    friends=friends->next;//incrementa l'efficenza dell'applicazione.. mantengo sempre SO_FRIENDS_NUM
  } 
}