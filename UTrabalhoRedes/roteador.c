#include "funcoes.h"

void *receber(void *arg);
void *processar(void *arg);
void *enviar(void *arg);
void *atualizar(void *arg);
void *timeout(void *arg);

pthread_mutex_t mt_bufferTimeout = PTHREAD_MUTEX_INITIALIZER, mt_bufferSaida = PTHREAD_MUTEX_INITIALIZER, mt_bufferEntrada = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mt_log = PTHREAD_MUTEX_INITIALIZER, mt_msgLog = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mt_tabelaRoteamento = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char const *argv[]){
	LocalInfo info;
	int op = 1;
	Pacote *pacote = (Pacote *)malloc(sizeof(Pacote));

	if(argc != 2){
		printf("ERRO! Informe o arquivo executável e o número do roteador.\n");
		return 0;
	}

	inicializaRoteador(&info, char2int(argv[1]));

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
		// Recebe as mensagens do socket
		if(recvfrom(sckt, &pacote, sizeof(pacote), 0, (struct sockaddr *)&socket_addr, (uint *)&s_len) == -1){
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
