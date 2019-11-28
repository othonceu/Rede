#include "funcoes.h"

Roteador *getRoteador(Roteador *r, int id){
	while(r != NULL){
		if(r->id == id){
			return r;
		}
		r = r->prox;
	}
	return NULL;
}

void pushRoteadores(Roteador **roteadores, int id, int porta, char ip[20]){
	Roteador *r = *roteadores, *novo = (Roteador *)malloc(sizeof(Roteador));
	novo->id = id;
	novo->porta = porta;
	strcpy(novo->ip, ip);
	novo->prox = r;
	*roteadores = novo;
}

void imprimirRoteadores(Roteador *r){
	while(r != NULL){
		printf("ID: %d\tPorta: %d\tIP: %s\n", r->id, r->porta, r->ip);
		r = r->prox;
	}
}

void pushTopologia(Topologia **topologia, int id, int distancia){
	Topologia *t = *topologia, *novo = (Topologia *)malloc(sizeof(Topologia));
	novo->idRoteador = id;
	novo->distancia = distancia;
	novo->prox = t;
	*topologia = novo;
}

void imprimirTopologia(Topologia *t){
	while(t != NULL){
		printf("ID0: %d\tDistancia: %d\n", t->idRoteador, t->distancia);
		t = t->prox;
	}
}

void pushListaEspera(ListaEspera **lista, Pacote pacote, int tentativas, clock_t tempo, pthread_mutex_t *mutex){
	pthread_mutex_lock(mutex);
	int cont = 0;
	ListaEspera *l = *lista, *novo = (ListaEspera *)malloc(sizeof(ListaEspera));
	novo->pacote = pacote;
	novo->tentativas = tentativas;
	novo->inicio = tempo;
	novo->prox = NULL;
	if (*lista == NULL){
		*lista = novo;
		pthread_mutex_unlock(mutex);
		return;
	}
	while(l->prox != NULL){
		if(cont >= MAX_BUFFER){
			printf("Buffer Cheio\n");
			pthread_mutex_unlock(mutex);
			return;
		}
		cont++;
		l = l->prox;
	}
	l->prox = novo;
	pthread_mutex_unlock(mutex);
}

void popListaEspera(ListaEspera **lista, pthread_mutex_t *mutex){
	pthread_mutex_lock(mutex);
	ListaEspera *l = *lista;
	*lista = l->prox;
	free(l);
	pthread_mutex_unlock(mutex);
}

int removerListaEspera(ListaEspera **lista, Pacote *pacote, pthread_mutex_t *mutex){
	pthread_mutex_lock(mutex);
	int id;
	ListaEspera *l = *lista, *aux = l;
	if(l == NULL){
		pthread_mutex_unlock(mutex);
		return -1;
	}
	if(l->pacote.ack == pacote->ack){
		id = pacote->idOrigem;
		*lista = l->prox;
		free(l);
		pthread_mutex_unlock(mutex);
		return id;
	}else{
		while(l != NULL){
			if(l->pacote.ack == pacote->ack){
				id = pacote->idOrigem;
				aux->prox = l->prox;
				free(l);
				pthread_mutex_unlock(mutex);
				return id;
			}
			aux = l;
			l = l->prox;
		}
	}
	pthread_mutex_unlock(mutex);
	return -1;
}

void imprimirLista(ListaEspera *lista){
	while(lista != NULL){
		printf("IIIIII: %s\n", lista->pacote.msg);
		lista = lista->prox;
	}
}

void imprimirPacote(Pacote *pacote){
	printf("\n\n------------------------------------ Pacote ------------------------------------\n");
	printf("Tipo: %d\n", pacote->tipo);
	printf("ACK: %d\n", pacote->ack);
	if(pacote->tipo == 0){
		printf("MSG: %s\n", pacote->msg);
	}
	printf("--------------------------------------------------------------------------------\n");
}

int char2int(char const *str){		// Converte char para int
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

void lerRoteadores(LocalInfo *info){
	FILE *arq = fopen("roteador.config", "r");		// Abre o arquivo com o roteador.congig
	int porta, id;
	char ip[20];
	if(arq == NULL){
		printf("Não foi possivel abrir o arquivo\n");
		fclose(arq);
		return;
	}
	while(fscanf(arq, "%d %d %s", &id, &porta, ip) != EOF){		// Lê os dados do arquivo
		pushRoteadores(&info->roteadores, id, porta, ip);
		if(info->id == id){
			info->porta = porta;
			strcpy(info->ip, ip);
		}
	}
	fclose(arq);
}

void lerTopologia(LocalInfo *info, Topologia **vizinhos){
	FILE *arq = fopen("enlaces.config", "r");		// Abre o arquivo com o enlace da rede
	int id_0, id_1, distancia;
	if(arq == NULL){
		printf("Não foi possivel abrir o arquivo\n");
		fclose(arq);
		return;
	}
	while(fscanf(arq, "%d %d %d", &id_0, &id_1, &distancia) != EOF){		// Lê os dados do arquivo
		if(id_0 == info->id){
			pushTopologia(vizinhos, id_1, distancia);
		}else if(id_1 == info->id){
			pushTopologia(vizinhos, id_0, distancia);
		}
	}
	fclose(arq);
}

Topologia *getTopologia(Topologia *topologia, int id){
	while(topologia != NULL){
		if(topologia->idRoteador == id){
			return topologia;
		}
		topologia = topologia->prox;
	}
	return NULL;
}

void inicializarTabela(LocalInfo *info){
	TabelaRoteamento *t = (TabelaRoteamento *)malloc(sizeof(TabelaRoteamento));
	Topologia *vizinhos = NULL;
	int i = 0;

	for(i = 0; i < MAX_ROUT; i++){
		t->vDist[i].idRoteador = -1;
		t->vDist[i].distancia = INT_MAX;
		t->proxSalto[i] =  -1;
	}

	lerTopologia(info, &vizinhos);
	info->topologia = vizinhos;
	i = 0;
	while(vizinhos != NULL){
		t->vDist[i].idRoteador = vizinhos->idRoteador;
		t->vDist[i].distancia = vizinhos->distancia;
		t->proxSalto[i] =  vizinhos->idRoteador;
		vizinhos = vizinhos->prox;
		i++;
	}
	info->tabela = t;
	setPosicaoTabela(info, info->id, 0, info->id, 0);
}

void inicializaRoteador(LocalInfo *info, int id){
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

void bellmanFord(LocalInfo *info, Pacote *pacote){
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
	//system("clear");
	printf("\n\n _______________________________________________________________________ \n");
	printf("| \tNº da Opção\t\t| \t\tOpção\t\t\t|\n");
	printf("|-------------------------------|---------------------------------------|\n");
	printf("|→ 0\t\t\t\t| Sair\t\t\t\t\t|\n");
	printf("|→ 1\t\t\t\t| Enviar Mensagem\t\t\t|\n");
	printf("|→ 2\t\t\t\t| Mostrar Informações dos Roteadores\t|\n");
	printf("|→ 3\t\t\t\t| Mostrar Tabela\t\t\t|\n");
	printf("|→ 4\t\t\t\t| Mostrar Mensagens Recebidas\t\t|\n");
	printf("|→ 5\t\t\t\t| Mostrar LOG\t\t\t\t|\n");
	printf("|_______________________________________________________________________|\n");
	printf("→ ");
}
