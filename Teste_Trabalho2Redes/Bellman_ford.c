//------------------------------------------------------------------------------
// Os algoritmos de roteamento exibem o rede como um gráfico
//
// Problema: encontre o menor custo caminho entre dois nós
//
// Fatores:
// 	Topologia estática
// 	Carga dinâmica
// 	Política
//
// Duas abordagens principais:
// 	Protocolo de vetor de distância
// 	Protocolo do estado do link
//---------------------------------------------------------------------------
// Condições de início:
//	• Cada roteador começa com um vetor de distâncias para todos os redes
//
// Enviar etapa:
//	• Cada roteador anuncia seu vetor atual para todos os roteadores vizinhos
//
// Etapa de recebimento:
//	• Para cada rede X, o roteador encontra a menor distância para X
//		• Considera a distância atual até X
//		• Em seguida, leva em consideração a distância a X de seus vizinhos
//	• Roteador atualiza seu custo para X
//	• Depois de fazer isso para todos os X, o roteador envia a etapa
//-------------------------------------------------------------------------------



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define inf 123456789

#define capRot 6

typedef struct{
                                             
  int grafo[capRot][capRot];
  int numRot ;
                                                            
}Tab;



int main(){
     	
	Tab *tb;


	tb = (Tab*) malloc (sizeof(Tab));
		



	return 0;
}





























































/*

void iniciar_Enlace(Tab *tab){
	int x, y,opt;
	int i,j;

	 FILE *file = fopen("enlaces.config", "r");

	  if(file){
	  	for (int i = 0; fscanf(file, "%d %d %d", &x, &y, &opt) != EOF; i++){

		 	tab->numRot = i;
		}
	  	fclose(file);
   	   }

	for(i = 0; i < tab->numRot; i++){
		
		for(j = 0 ; j < tab->numRot; j ++){
				
				//if(i==j) tab->grafo[i][j] = 0;
				
				else tab->grafo[i][j] = inf;
					
		}

	}


}

 
void criar_Enlace( Tab *tab){
  int x, y, peso;

  FILE *file = fopen("enlaces.config", "r");

  if (file){
    for (int i = 0; fscanf(file, "%d %d %d", &x, &y, &peso) != EOF; i++){
      tab->grafo[x][y] = peso;
      tab->grafo[y][x] = peso;
	
      tab->numRot = i;
    }
  
    fclose(file);
  }
}

int main(){
	int i,j;
	Tab *Tb;
	
    Tb = (Tab*) malloc(sizeof(Tab));

	
	iniciar_Enlace(Tb);

	for(i = 0; i < Tb->numRot; i++){
		
		for(j = 0 ; j < Tb->numRot; j ++){
				
				if(i==j) Tb->grafo[i][j] = 0;
				
			printf("!!%d %d : %d \n", i, j, Tb->grafo[i][j]);
			
			
		}

	}
	
	criar_Enlace(Tb);
	

	for(i = 0; i < Tb->numRot; i++){
		
		for(j = 0 ; j < Tb->numRot; j ++){
				
				if(i==j) Tb->grafo[i][j] = 0;
			
			printf("------------------------\n");	
			printf("%d %d : %d \n", i, j, Tb->grafo[i][j]);			
		}
	}

*/
