#ifndef DEFAULTCONF
#define DEFAULTCONF
//--------------------- valori di default per i parametri di configurazione 
//--------------------- letti a tempo di esecuzione da stdin

//parametro					                        “Myconf”
#define DEFAULT_SO_USERS_NUM					    100
#define DEFAULT_SO_NODES_NUM					    15
#define DEFAULT_SO_BUDGET_INIT					    200
#define DEFAULT_SO_REWARD                       	10
#define DEFAULT_SO_MIN_TRANS_GEN_NSEC               200000000
#define DEFAULT_SO_MAX_TRANS_GEN_NSEC           	300000000
#define DEFAULT_SO_RETRY					        5
#define DEFAULT_SO_TP_SIZE					        150
#define DEFAULT_SO_MIN_TRANS_PROC_NSEC          	10000000
#define DEFAULT_SO_MAX_TRANS_PROC_NSEC          	20000000
#define DEFAULT_SO_SIM_SEC                      	10
#define DEFAULT_SO_FRIENDS_NUM					    5
#define DEFAULT_SO_HOPS					            5

//grandissimo TP_SIZE, non verrano probabilmente creati nuovi nodi perchè difficilmente verranno raggiunte le capienze delle pool
//parametro					                        “conf1”
#define DEFAULT1_SO_USERS_NUM					    100
#define DEFAULT1_SO_NODES_NUM					    10
#define DEFAULT1_SO_BUDGET_INIT					    1000  
#define DEFAULT1_SO_REWARD                       	1
#define DEFAULT1_SO_MIN_TRANS_GEN_NSEC              200000000
#define DEFAULT1_SO_MAX_TRANS_GEN_NSEC           	300000000
#define DEFAULT1_SO_RETRY					        20
#define DEFAULT1_SO_TP_SIZE					        1000
#define DEFAULT1_SO_MIN_TRANS_PROC_NSEC          	10000000
#define DEFAULT1_SO_MAX_TRANS_PROC_NSEC          	20000000
#define DEFAULT1_SO_SIM_SEC                      	10
#define DEFAULT1_SO_FRIENDS_NUM					    3
#define DEFAULT1_SO_HOPS				            10

//piccolissimio TP_SIZE, di solito vengono creati molti nuovi nodi per gestire le transizioni scartate
//parametro					                        “conf2”
#define DEFAULT2_SO_USERS_NUM					    100
#define DEFAULT2_SO_NODES_NUM					    10
#define DEFAULT2_SO_BUDGET_INIT					    1000  
#define DEFAULT2_SO_REWARD                       	20
#define DEFAULT2_SO_MIN_TRANS_GEN_NSEC              20000000
#define DEFAULT2_SO_MAX_TRANS_GEN_NSEC           	30000000
#define DEFAULT2_SO_RETRY					        2
#define DEFAULT2_SO_TP_SIZE					        20
#define DEFAULT2_SO_MIN_TRANS_PROC_NSEC          	1000000
#define DEFAULT2_SO_MAX_TRANS_PROC_NSEC          	2000000
#define DEFAULT2_SO_SIM_SEC                      	20
#define DEFAULT2_SO_FRIENDS_NUM					    5
#define DEFAULT2_SO_HOPS					        2

//pochi users, di solito si arresta perche tutti gli utenti sono terminati
//parametro					                        “conf3”       
#define DEFAULT3_SO_USERS_NUM					    20
#define DEFAULT3_SO_NODES_NUM					    10
#define DEFAULT3_SO_BUDGET_INIT					    10000  
#define DEFAULT3_SO_REWARD                       	1
#define DEFAULT3_SO_MIN_TRANS_GEN_NSEC              20000000
#define DEFAULT3_SO_MAX_TRANS_GEN_NSEC           	30000000
#define DEFAULT3_SO_RETRY					        10
#define DEFAULT3_SO_TP_SIZE					        100
#define DEFAULT3_SO_MIN_TRANS_PROC_NSEC          	1000000
#define DEFAULT3_SO_MAX_TRANS_PROC_NSEC          	2000000
#define DEFAULT3_SO_SIM_SEC                      	20
#define DEFAULT3_SO_FRIENDS_NUM					    3
#define DEFAULT3_SO_HOPS				            10


#endif
