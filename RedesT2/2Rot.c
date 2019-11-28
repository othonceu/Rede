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

#define Max 123456789
#define MSG_SIZE 100
#define MAX_BUFFER 100
#define MAX_ROUT 10
#define TIMEOUT 0.150

pthread_t t_enviar, t_receber, t_processar, t_atualizar, t_timeout;

void *receber(void *arg);
void *processar(void *arg);
void *enviar(void *arg);
void *atualizar(void *arg);
void *timeout(void *arg);

pthread_mutex_t mt_bufferTimeout = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mt_bufferSaida = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mt_bufferEntrada = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mt_log = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mt_bufferSaida = mt_msgLog = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mt_tabelaRoteamento = PTHREAD_MUTEX_INITIALIZER;

typedef struct{
	int idRoteador;
	int distancia;
}VetorDistancia;

typedef struct RT{
	int id;
	char ip[20];
	int porta;
	struct RT *prox;
}Roteador;


typedef struct Pacote{
	int idInfo;
	int tipo;		
	int ack;	 
	VetorDistancia vet0or_distancia[MAX_ROUT];
	int idDestino;	
	int idOrigem;	
	char msg[MSG_SIZE];
	int idRoteadorTP;
	int distanciaTP;
	struct Pacote *proxTP;
	struct Pacote pacote;
	int tentativaLE;
	clock_t inicioLE;
      char msgLog [MSG_SIZE];
	struct pacote *proxBDLog;
	VetorDistancia vDistTAB[MAX_ROUT];
	int proxSaltoTAB[MAX_ROUT];
	int portaInfo;
	int ackInfo;
	int ipInfo[20];
	struct pacote *bufferSaidaInfo, *bufferEntradaInfo, *bufferTimeoutInfo;
	Roteador *roteadoresInfo;
	struct pacote *msgInfo;
	struct pacote *logInfo;
	struct pacote *tabelaInfo;
}Pac;

Roteador *getRoteador(Roteador *r, int id){	
	while(r != NULL){
		if(r->id == id) return r;
		
		r = r->prox;
	}

	return NULL;
}

void pushRoteadores(Roteador **roteadores, int id, int porta, char ip[20]){
	Roteador *r = *roteadores;
	Roteador  *novo = (Roteador *)malloc(sizeof(Roteador));
	novo->id = id;
	novo->porta = porta;
	strcpy(novo->ip, ip);
	novo->prox = r;
	*roteadores = novo;
}

void imprimirRoteadores(Roteador *r){ // verificar se essa função é necessária
	while(r != NULL){
		printf("ID: %d\tPorta: %d\tIP: %s\n", r->id, r->porta, r->ip);
		r = r->prox;
	}
}

void pushTopologia(Pac **topologia, int id, int distancia){
	Pac *t = *topologia; 
	Pac *novo = (Pac *)malloc(sizeof(Pac));
	novo->idRoteadorTP = id;
	novo->distancia = distancia;
	novo->proxTP = t;
	*topologia = novo;
}

void imprimirTopologia(Pac *t){
	while(t != NULL){
		printf("ID0: %d\tDistancia: %d\n", t->idRoteadorTP, t->distanciaTP);
		t = t->proxTP;
	}
}

void pushListaEspera(Pac **lista, Pacote pacote, int tentativas, clock_t tempo, pthread_mutex_t *mutex){
	pthread_mutex_lock(mutex);
	int cont = 0;
	Pac *l = *lista;
	Pac *novo = (Pac *)malloc(sizeof(Pac));
	novo->pacote = pacote;
	novo->tentativasLE = tentativas;
	novo->inicio = tempo;
	novo->proxLE = NULL;
	if (*lista == NULL){
		*lista = novo;
		pthread_mutex_unlock(mutex);
		return;
	}
	while(l->proxLE != NULL){
		if(cont >= MAX_BUFFER){
			printf("Buffer Cheio\n");
			pthread_mutex_unlock(mutex);
			return;
		}
		cont++;
		l = l->proxLE;
	}
	l->proxLE = novo;
	pthread_mutex_unlock(mutex);
}
void popListaEspera(Pac **lista, pthread_mutex_t *mutex){
	pthread_mutex_lock(mutex);
	 Pac *l = *lista;
	*lista = l->proxLE;
	free(l);
	pthread_mutex_unlock(mutex);
}

int removerListaEspera(Pac **lista, Pacote *pacote, pthread_mutex_t *mutex){
	pthread_mutex_lock(mutex);
	int id;
	Pac *l = *lista, *aux = l;
	if(l == NULL){
		pthread_mutex_unlock(mutex);
		return -1;
	}
	if(l->ack == pacote->ack){
		id = pacote->idOrigem;
		*lista = l->proxLE;
		free(l);
		pthread_mutex_unlock(mutex);
		return id;
	}else{
		while(l != NULL){
			if(l->ack == pacote->ack){
				id = pacote->idOrigem;
				aux->proxLE = l->proxLE;
				free(l);
				pthread_mutex_unlock(mutex);
				return id;
			}
			aux = l;
			l = l->proxLE;
		}
	}
	pthread_mutex_unlock(mutex);
	return -1;
}

void imprimirPacote(Pacote *pacote){
	printf("\n\n Pacote ------------------------------------\n");
	printf("Tipo: %d\n", pacote->tipo);
	printf("ACK: %d\n", pacote->ack);
	if(pacote->tipo == 0){
		printf("MSG: %s\n", pacote->msg);
	}
	printf("------------------------------------------\n");
}

int char2int(char const *str){		// Converte char para int (peguei na internet)
	int a = 0, cont = 1, i;
	for (i = strlen(str) - 1; i >= 0; i--, cont *= 10){
		a += cont * (str[i] - '0');
	}
	return a;
}

void inicializaSocket(struct sockaddr_in *socket_addr, int *sckt, int porta){	// Inicializa o socket
	int s_len = sizeof(socket_addr);
	if((*sckt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
		printf("Falha ao criar Socket.\n");
		exit(1);
	}else{
		memset((char *) socket_addr, 0, s_len);
		socket_addr->sin_family = AF_INET;
		socket_addr->sin_port = htons(porta); 
		socket_addr->sin_addr.s_addr = htonl(INADDR_ANY); 
	}
}


void lerRoteadores(Pac *info){
	FILE *arq = fopen("roteador.config", "r");		// Abre o arquivo com o roteador.congig
	int porta, id;
	char ip[20];
	if(arq == NULL){
		printf("Não foi possivel abrir o arquivo\n");
		fclose(arq);
		return;
	}
	while(fscanf(arq, "%d %d %s", &id, &porta, ip) != EOF){		// Lê os dados do arquivo
		pushRoteadores(&info->roteadoresInfo, id, porta, ip);
		if(info->idInfo == id){
			info->portaInfo = porta;
			strcpy(info->ipInfo, ip);
		}
	}
	fclose(arq);
}

void lerTopologia(Pac *info, Pac **vizinhos){
	FILE *arq = fopen("enlaces.config", "r");		// Abre o arquivo com o enlace da rede
	int id_0, id_1, distancia;
	
	if(arq == NULL){
		printf("Não foi possivel abrir o arquivo\n");
		fclose(arq);
		return;
	}
	while(fscanf(arq, "%d %d %d", &id_0, &id_1, &distancia) != EOF){		// Lê os dados do arquivo
		if(id_0 == info->idInfo){
			pushTopologia(vizinhos, id_1, distancia);
		}else if(id_1 == info->idInfo){
			pushTopologia(vizinhos, id_0, distancia);
		}
	}
	fclose(arq);
}

Pac *getTopologia(Pac *topologia, int id){
	while(topologia != NULL){
		if(topologia->idRoteador == id){
			return topologia;
		}
		topologia = topologia->proxTP;
	}
	return NULL;
}

//--------------------------------------

void inicializarTabela(Pac *info){
	Pac *t = (Pac *)malloc(sizeof(Pac));
	Pac *vizinhos = NULL;
	int i = 0;

	for(i = 0; i < MAX_ROUT; i++){
		t->vDist[i].idRoteador = -1;
		t->vDist[i].distancia = Max;
		t->proxSalto[i] =  -1;
	}

	lerTopologia(info, &vizinhos);
	info->topologia = vizinhos;
	i = 0;
	while(vizinhos != NULL){
		t->vDist[i].idRoteador = vizinhos->idRoteador;
		t->vDist[i].distancia  = vizinhos->distancia;
		t->proxSalto[i] =  vizinhos->idRoteador;
		vizinhos = vizinhos->proxTP;
		i++;
	}
	info->tabela = t;
	setPosicaoTabela(info, info->id, 0, info->id, 0);
}

void inicializaRoteador(Pac *info, int id){
	printf("ID: %d\n", id);
	info->id = id;
	info->bufferSaida = NULL;
	info->bufferEntrada = NULL;
	info->bufferTimeout = NULL;
	info->roteadores = NULL;
	info->topologia = NULL;
	info->msg = NULL;
	info->log = NULL;
	info->ack = 0;
	info->tabela = NULL;

	lerRoteadores(info);
	inicializarTabela(info);
}

void pushLog(Log **log, char *msg, pthread_mutex_t *mutex){
	pthread_mutex_lock(mutex);
	Log *l = *log, *novo = (Log *)malloc(sizeof(Log));
	strcpy(novo->msg, msg);
	novo->prox = l;
	*log = novo;
	pthread_mutex_unlock(mutex);
}

void imprimirMSG(Log *log){
	while(log != NULL){
		printf("MSG: %s\n", log->msg);
		log = log->prox;
	}
}

Pacote *configurarPacote(int tipo, VetorDistancia *vetor_distancia, int idDestino, int idOrigem, char *msg){
	Pacote *novo = (Pacote *)malloc(sizeof(Pacote));
	novo->tipo = tipo;
	novo->idDestino = idDestino;
	novo->idOrigem = idOrigem;
	strcpy(novo->msg, msg);
	if(tipo == 2){
		for(int i = 0; i < MAX_ROUT; i++){
			novo->vetor_distancia[i].idRoteador = vetor_distancia[i].idRoteador;
			novo->vetor_distancia[i].distancia = vetor_distancia[i].distancia;
		}
	}else{
		memset(novo->vetor_distancia, -1, sizeof(novo->vetor_distancia));
	}
	return novo;
}

int getPosicaoTabela(LocalInfo *info, int id){
	for(int i = 0; i < MAX_ROUT; i++){
		if(info->tabela->vDist[i].idRoteador == id){
			return i;
		}
	}
	return -1;
}

int setPosicaoTabela(LocalInfo *info, int id, int distancia, int proxSalto, int timeout){
	int posicao = getPosicaoTabela(info, id), alterou = 0;
	if(timeout == 1){
		for(int i = 0; i < MAX_ROUT; i++){
			if(info->tabela->proxSalto[i] == info->tabela->vDist[posicao].idRoteador){
				info->tabela->vDist[i].distancia = INT_MAX;
				info->tabela->proxSalto[i] = -1;
			}else if(info->tabela->vDist[i].idRoteador == info->tabela->vDist[posicao].idRoteador){
				alterou = info->tabela->proxSalto[posicao];
				info->tabela->vDist[i].distancia = INT_MAX;
				info->tabela->proxSalto[i] = -1;
			}
		}
		return alterou;
	}
	if(posicao == -1){
		for(int i = 0; i < MAX_ROUT; i++){
			if(info->tabela->vDist[i].idRoteador == -1){
				info->tabela->vDist[i].idRoteador = id;
				info->tabela->vDist[i].distancia = distancia;
				info->tabela->proxSalto[i] = proxSalto;
				return -1;
			}
		}
	}else{
		if(info->tabela->vDist[posicao].distancia > distancia){
			info->tabela->vDist[posicao].distancia = distancia;
			info->tabela->proxSalto[posicao] = proxSalto;
		}
	}
	return -1;
}

void bellmanFord(LocalInfo *info, Pacote *pacote){ // mudança de algoritmo (apenas enlace adjacente)
	int posicao = getPosicaoTabela(info, pacote->idOrigem), i;
	if(info->tabela->vDist[posicao].distancia == INT_MAX){
		return;
	}
	for(i = 0; i < MAX_ROUT; i++){
		if(pacote->vetor_distancia[i].idRoteador == -1 || pacote->vetor_distancia[i].distancia == INT_MAX){
			continue;
		}
		setPosicaoTabela(info, 
						pacote->vetor_distancia[i].idRoteador,
						pacote->vetor_distancia[i].distancia + info->tabela->vDist[posicao].distancia,
						info->tabela->vDist[posicao].idRoteador,
						0);
	}
}

void imprimirTabelaRoteamento(TabelaRoteamento *tabela){
	printf("\n\n------------------------------------ Tabela Roteamento ------------------------------------\n");
	for(int i = 0; i < MAX_ROUT; i++){
		if(tabela->vDist[i].idRoteador != -1){
			printf("ID: %d\t|", tabela->vDist[i].idRoteador);
			printf("Dist: %d\t|", tabela->vDist[i].distancia);
			printf("Prox Salto: %d\n", tabela->proxSalto[i]);
		}
	}
	printf("\n\n-------------------------------------------------------------------------------------------\n");
}

void imprimirVeetorDistancia(VetorDistancia *v){
	printf("-------------Vetor-------------\n");
	for (int i = 0; i < MAX_ROUT; ++i){
		printf("|\t%d\t|\t%d\t|\n", v[i].idRoteador, v[i].distancia);
	}
	printf("-------------------------------\n");
}

void menu(){

	printf("\n 1 Enviar Mensagem \n");
	printf(" 2 Mostrar Informações dos Roteadores\n");
	printf(" 3 Mostrar Tabela \n");
	printf(" 4 Mostrar Mensagens Recebidas\n");
	printf(" 5 Mostrar LOG\n");
	printf(" 0 Sair\n");
}

void *enviar(void *arg){
	LocalInfo *info = (LocalInfo*)arg;

	struct sockaddr_in socket_addr;
	int sckt, s_len = sizeof(socket_addr), tentativas;
	clock_t tempo;
	char log[50], aux[2];
	Roteador *r;
	Pacote pacote;
	aux[1] = '\0';

	while(1){
		if(info->bufferSaida == NULL){
			continue;
		}
		pacote = info->bufferSaida->pacote;
		r = getRoteador(info->roteadores, info->bufferSaida->pacote.idDestino);

		if(r == NULL){
			printf("ERRO! Roteador não existe!\n");
			popListaEspera(&info->bufferSaida, &mt_bufferSaida);
			continue;
		}

		inicializaSocket(&socket_addr, &sckt, r->porta);
		if(inet_aton(r->ip , &socket_addr.sin_addr) == 0){ // Verifica se o endereço de IP é valido
			printf("Endereço de IP invalido\n");
			exit(1);
		}

		if(pacote.tipo == 0 || pacote.tipo == 2){
			pacote.ack = info->ack;
			info->ack++;
		}

		strcpy(log, "Pacote de ");
		if(info->bufferSaida->pacote.tipo == 0){
			strcat(log, "dados");
			strcat(log, " enviado para [");
			aux[0] = (info->bufferSaida->pacote.idDestino) + '0';
			strcat(log, aux);
			strcat(log, "]");
			strcat(log, "por [");
			aux[0] = (r->id) + '0';
			strcat(log, aux);
			strcat(log, "]");
			pushLog(&info->log, log, &mt_log);
		}else if(info->bufferSaida->pacote.tipo == 1){
			strcat(log, "confirmação");
		}else{
			strcat(log, "controle");
		}

		if(sendto(sckt, &pacote, sizeof(pacote) , 0 , (struct sockaddr *)&socket_addr, s_len) == -1){ // Envia a mensagem
			printf("Não foi possivel enviar a mensagem.\n");
			exit(1);
		}

		if(info->bufferSaida->pacote.tipo == 0 || info->bufferSaida->pacote.tipo == 2){
			tentativas = info->bufferSaida->tentativas;
			tempo = clock();
			pushListaEspera(&info->bufferTimeout, pacote, tentativas, tempo, &mt_bufferTimeout);
		}
		popListaEspera(&info->bufferSaida, &mt_bufferSaida);
	}	
}

void *atualizar(void *arg){
	LocalInfo *info = (LocalInfo*)arg;
	Topologia *temp;
	Pacote *pacote = (Pacote *)malloc(sizeof(Pacote));
	while(1){
		temp = info->topologia;
		while(temp != NULL){
			pacote = configurarPacote(2, info->tabela->vDist, temp->idRoteador, info->id, "\0");
			pushListaEspera(&info->bufferSaida, *pacote, 0, 0, &mt_bufferSaida);
			temp = temp->prox;
		}
		sleep(2);
	}
}

void *processar(void *arg){
	LocalInfo *info = (LocalInfo*)arg;
	int id, temp;
	char log[50], aux[2];
	Topologia *vizinho;
	Pacote *pacote = (Pacote *)malloc(sizeof(Pacote));
	aux[1] = '\0';
	while(1){
		if(info->bufferEntrada == NULL){
			continue;
		}

		if(info->bufferEntrada->pacote.idDestino == info->id){
			if(info->bufferEntrada->pacote.tipo == 0){
				pushLog(&info->msg, info->bufferEntrada->pacote.msg, &mt_msgLog);

				strcpy(log, "Pacote de dados recebido de [");
				aux[0] = (info->bufferEntrada->pacote.idOrigem) + '0';
				strcat(log, aux);
				strcat(log, "]");
				pushLog(&info->log, log, &mt_log);

				pacote = configurarPacote(1, 0, info->bufferEntrada->pacote.idOrigem, info->id, "\0");
				pacote->ack = info->bufferEntrada->pacote.ack;
				pushListaEspera(&info->bufferSaida, *pacote, 0, 0, &mt_bufferSaida);
			}

			if(info->bufferEntrada->pacote.tipo == 1){

				strcpy(log, "Pacote de confirmação recebido de [");
				aux[0] = (info->bufferEntrada->pacote.idOrigem) + '0';
				strcat(log, aux);
				strcat(log, "]");
				pushLog(&info->log, log, &mt_log);

				id = removerListaEspera(&info->bufferTimeout, &info->bufferEntrada->pacote, &mt_bufferTimeout);
				if(id >= 0){
					vizinho = getTopologia(info->topologia, id);
					if(vizinho != NULL){
						setPosicaoTabela(info, id, vizinho->distancia, id, 0);
					}
				}
			}

			if(info->bufferEntrada->pacote.tipo == 2){

				strcpy(log, "Pacote do vetor de distancias recebido de [");
				aux[0] = (info->bufferEntrada->pacote.idOrigem) + '0';
				strcat(log, aux);
				strcat(log, "]");
				pushLog(&info->log, log, &mt_log);

				pacote = configurarPacote(1, 0, info->bufferEntrada->pacote.idOrigem, info->id, "\0");
				pacote->ack = info->bufferEntrada->pacote.ack;
				pushListaEspera(&info->bufferSaida, *pacote, 0, 0, &mt_bufferSaida);

				bellmanFord(info, &info->bufferEntrada->pacote);
			}

			if(info->bufferEntrada->pacote.tipo == 3){
				for(int i = 0; i < MAX_ROUT; i++){
					if(info->tabela->proxSalto[i] == info->bufferEntrada->pacote.idOrigem){
						temp = setPosicaoTabela(info, info->bufferEntrada->pacote.vetor_distancia[0].idRoteador, -1, -1, 1);
						if(temp == info->bufferEntrada->pacote.idOrigem){
							setPosicaoTabela(info, temp, -1, -1, 1);
						}
					}
				}
			}
		}
		popListaEspera(&info->bufferEntrada, &mt_bufferEntrada);
	}
}

void *receber(void *arg){
	LocalInfo *info = (LocalInfo*)arg;
	struct sockaddr_in socket_addr;
	int  sckt, s_len = sizeof(socket_addr);
	Pacote pacote;

	inicializaSocket(&socket_addr, &sckt, info->porta);

	if(bind(sckt, (struct sockaddr *) &socket_addr, s_len) == -1){	// Liga um nome ao socket
		printf("A ligacao do socket com a porta falhou.\n");
		exit(1);
	}

	while(1){
		if(recvfrom(sckt, &pacote, sizeof(pacote), 0, (struct sockaddr *)&socket_addr, (uint *)&s_len) == -1){// Recebe as mensagens do socket
			printf("ERRO ao receber os pacotes.\n");
		}
		pushListaEspera(&info->bufferEntrada, pacote, 0, 0, &mt_bufferEntrada);
	}
}

void *timeout(void * arg){
	LocalInfo *info = (LocalInfo*)arg;
	ListaEspera *temp;
	int tentativas;
	clock_t inicio, tempo;
	char log[50], aux[2];
	Pacote pacote, *tempPacote;
	aux[1] = '\0';
	Topologia *vizinho;
	while(1){
		pthread_mutex_lock(&mt_bufferTimeout);
		if(info->bufferTimeout == NULL){
			pthread_mutex_unlock(&mt_bufferTimeout);
			continue;
		}
		temp = info->bufferTimeout;
		pthread_mutex_unlock(&mt_bufferTimeout);

		inicio =  temp->inicio;
		pacote = temp->pacote;
		tentativas = temp->tentativas;
		
		tempo = clock();
		if((double)(tempo - inicio)/CLOCKS_PER_SEC > TIMEOUT){

			if(pacote.tipo == 0){
				strcpy(log, "Pacote enviado para [");
				aux[0] = (pacote.idDestino) + '0';
				strcat(log, aux);
				strcat(log, "] recebeu TIMEOUT");
				pushLog(&info->log, log, &mt_log);
			}else if(pacote.tipo == 2 && tentativas >= 2){
				setPosicaoTabela(info, pacote.idDestino, -1, -1, 1);

				vizinho = info->topologia;
				while(vizinho != NULL){
					if(vizinho->idRoteador != pacote.idDestino){
						tempPacote = configurarPacote(3, 0, vizinho->idRoteador, info->id, "\0");
						tempPacote->vetor_distancia[0].idRoteador = pacote.idDestino;
						pushListaEspera(&info->bufferSaida, *tempPacote, 0, 0, &mt_bufferSaida);
					}
					vizinho = vizinho->prox;
				}

				strcpy(log, "Roteador [");
				aux[0] = (pacote.idDestino) + '0';
				strcat(log, aux);
				strcat(log, "] esta morto!");
				//pushLog(&info->log, log, &mt_log);
			}

			if((pacote.tipo == 0 || pacote.tipo == 2) && tentativas < 2){
				pushListaEspera(&info->bufferSaida, pacote, tentativas + 1, 0, &mt_bufferSaida);
			}
			popListaEspera(&info->bufferTimeout, &mt_bufferTimeout);
		}
	}
}




int main(int argc, char const *argv[]){
	LocalInfo info;
	int op = 1;
	Pacote *pacote = (Pacote *)malloc(sizeof(Pacote));

	if(argc != 2){
		printf("ERRO! Informe o arquivo executável e o número do roteador.\n");
		return 0;
	}

	inicializaRoteador(&info, char2int(argv[1]));   // vai inicializaRoteador LocalInfo

	pthread_create(&t_enviar, NULL, &enviar, &info);
	pthread_create(&t_receber, NULL, &receber, &info);
	pthread_create(&t_processar, NULL, &processar, &info);
	pthread_create(&t_atualizar, NULL, &atualizar, &info);
	pthread_create(&t_timeout, NULL, &timeout, &info);

	while(op){
		menu();
		scanf("%d", &op);

		switch(op){
			case 1:
				printf("Destino: ");
				scanf("%d", &pacote->idDestino);
				getchar();
				printf("MSG: ");
				scanf("%[^\n]s", pacote->msg);
				pacote = configurarPacote(0, 0, pacote->idDestino, info.id, pacote->msg);
				pushListaEspera(&info.bufferSaida, *pacote, 0, 0, &mt_bufferSaida);
				break;
			case 2:
				imprimirRoteadores(info.roteadores);
				break;
			case 3:
				imprimirTabelaRoteamento(info.tabela);
				break;
			case 4:
				imprimirMSG(info.msg);
				break;
			case 5:
				imprimirMSG(info.log);
				break;
			default:
				break;
		}
	}
	pthread_cancel(t_enviar);
	pthread_cancel(t_receber);
	pthread_cancel(t_processar);
	pthread_cancel(t_atualizar);
	pthread_cancel(t_timeout);
}








