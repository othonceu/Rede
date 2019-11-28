#! /bin/bash

trap ctrl_c INT

function ctrl_c() {
	echo -e "\nFinalizando programa!"
}

gcc -c funcoes.c -o funcoes.o -Wall

gcc roteador.c funcoes.o -lpthread -Wall

FILE=a.out
if [ -e "$FILE" ]
then
	./a.out 1
	rm -v a.out
else
	echo "Erro na hora de compilar o arquivo!"
fi
