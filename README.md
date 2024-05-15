# Blockchain Project in C
This repository contains the implementation of a blockchain project in the C language.

Environment: **Linux**.

Grade: **30/30**.

## Main Information

The project is based on user and node processes created through cycles of fork() and execve() from the master. Synchronization at startup is managed using semaphores to maximize concurrency. `Message queues`, `shared memory`, `semaphores`, and `signals` have been used to facilitate communication and data sharing between processes. Additionally, the **Readers-Writers** mechanism has been implemented for sharing the ledger, ensuring that multiple users can read simultaneously while only one node at a time can write, preventing potential conflicts and ensuring data integrity.

## Detailed Description
Below are details on the main implementation features:

### Startup Synchronization
- Usage of 2 semaphores to maximize concurrency:
  - The "all ready" semaphore is set to the number of children (SO_USERS_NUM + SO_NODES_NUM). The master waits for all children to be ready.
  - Each process decrements this semaphore at startup.
  - Subsequently, to maximize concurrency among users, each user waits for the release of a second semaphore: "all free", initialized by the master.

### Configuration Parameters
- Ability to input configuration parameters at runtime through various modes: default, preset values, or manual input from the keyboard.
- Parameters are stored in a shared structure in shared memory (shm), making them accessible to users and nodes.

### Inter-Process Communication
- Usage of shared arrays via shared memory (shm) to share process IDs between processes.
- Communication of node friend IDs to node processes via message queue.
- Implementation of a message queue for sending/receiving transactions from user to node.
- Sharing of the ledger via shared memory, implementing the Readers-Writers mechanism to ensure proper access management.

### Signal Handling
- Usage of various signals to manage system operation, such as SIGCHLD, SIGALARM, SIGURS1, SIGURS2, SIGTERM.
- Implementation of signal masks to block the reception of certain signals during critical operations.

### Other Features
- Implementation of an additional message queue to receive from the master the number of transactions present in the node's pool at the end of the summary.
- Periodic sending of transactions between nodes through the same message queue used by users.
- Usage of nanosleep() to handle inactive pauses during printing in master().
