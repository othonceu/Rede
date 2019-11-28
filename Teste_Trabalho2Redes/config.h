#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>

#define MSG_SIZE 100
#define MAX_BUFFER 100
#define TIMEOUT 0.150
#define MAX_ROUT 10

pthread_t t_enviar, t_receber, t_processar, t_atualizar, t_timeout;

typedef struct{
	int idRoteador;
	int distancia;
}VetorDistancia;

typedef struct{
	int tipo;		// 0 = dados e 1 = confirmação 2 = vetor de distancia
	int ack;		// Responsável por 
	VetorDistancia vetor_distancia[MAX_ROUT];
	int idDestino;	// Roteador Destino
	int idOrigem;	// Roteador Oerigem
	char msg[MSG_SIZE];	// Conteudo da mensagem
}Pacote;

typedef struct RT{
	int id;
	char ip[20];
	int porta;
	struct RT *prox;
}Roteador;

typedef struct TP{
	int idRoteador;
	int distancia;
	struct TP *prox;
}Topologia;

typedef struct LE{
	Pacote pacote;
	int tentativas;
	clock_t inicio;
	struct LE *prox;
}ListaEspera;

typedef struct BDLog{
	char msg[MSG_SIZE];
	struct BDLog *prox;
}Log;

typedef struct{
	VetorDistancia vDist[MAX_ROUT];
	int proxSalto[MAX_ROUT];
}TabelaRoteamento;

typedef struct{
	int id;
	int porta;
	int ack;
	char ip[20];
	ListaEspera *bufferSaida, *bufferEntrada, *bufferTimeout;
	Roteador *roteadores;
	Topologia *topologia;
	Log *msg;
	Log *log;
	TabelaRoteamento *tabela;
}LocalInfo;

#endif

