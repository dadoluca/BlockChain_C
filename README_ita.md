# Progetto Blockchain in C
Questo repository contiene l'implementazione di un progetto di blockchain in linguaggio C. 

Ambiente: **Linux**. 

Voto: **30/30**.

## Informazioni Principali

Il progetto si basa su processi user e node creati attraverso cicli di fork() ed execve() dal master. La sincronizzazione all'avvio è gestita tramite semafori per massimizzare il grado di concorrenza. Sono state utilizzate `code di messaggi`, `memoria condivisa`, `semafori` e `segnali` per realizzare la comunicazione e la condivisione di dati tra i processi. Inoltre, è stato implementato il meccanismo **Lettori-Scrittori** per la condivisione del libro mastro, garantendo che più utenti possano leggere contemporaneamente, mentre un solo nodo alla volta possa scriverci, prevenendo potenziali conflitti e garantendo l'integrità dei dati.

## Descrizione Dettagliata
Di seguito sono forniti dettagli sulle principali caratteristiche implementative:

### Sincronizzazione all'Avvio
- Utilizzo di 2 semafori per massimizzare la concorrenza: 
  - Il semaforo "pronti tutti" è impostato alla cardinalità dei figli (SO_USERS_NUM + SO_NODES_NUM). Il master attende che tutti i figli siano pronti.
  - Ogni processo decrementa questo semaforo all'avvio.
  - Successivamente, per massimizzare la concorrenza tra utenti, ogni user attende il rilascio di un secondo semaforo: "liberi tutti", inizializzato dal master.
  
### Parametri di Configurazione
- Possibilità di inserire i parametri di configurazione a tempo di esecuzione tramite diverse modalità: default, valori prestabiliti o inserimento manuale da tastiera.
- I parametri sono memorizzati in una struttura condivisa in memoria condivisa (shm), rendendoli accessibili da utenti e nodi.

### Comunicazione tra Processi
- Utilizzo di array condivisi tramite memoria condivisa (shm) per la condivisione dei pid tra processi.
- Comunicazione dei pid dei nodi friends ai processi nodo tramite message queue.
- Implementazione di una coda di messaggi per l'invio/ricezione di transizioni da utente a nodo.
- Condivisione del libro mastro tramite memoria condivisa, implementando il meccanismo di Lettori-Scrittori per garantire la corretta gestione degli accessi.

### Gestione dei Segnali
- Utilizzo di vari segnali per gestire il funzionamento del sistema, come SIGCHLD, SIGALARM, SIGURS1, SIGURS2, SIGTERM.
- Implementazione di maschere di segnali per bloccare la ricezione di alcuni segnali durante operazioni critiche.

### Altre Caratteristiche
- Implementazione di una coda di messaggi aggiuntiva per ricevere dal master il numero di transizioni presenti nella pool dei nodi.
- Invio periodico di transizioni tra nodi attraverso la stessa coda di messaggi utilizzata dagli utenti.
- Utilizzo di nanosleep() per la gestione delle pause inattive durante la stampa in master().
