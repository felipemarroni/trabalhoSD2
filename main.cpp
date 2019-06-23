#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <semaphore.h>
#include <netdb.h>


#define MAXLINE 11

using namespace std;

sem_t mutexAtivo;
sem_t mutexLider;



typedef struct mensagem{
	int tipo;
	int origem;
	int lider;
}mensagem;

typedef struct args{
	int id_processo;	//identificador deste processo
	int id_lider;		//identificador do lider
	int num_msg[6];		// onde num_msg[0] = Total de mensagens enviadas. num_msg[i] = numero de mensagens do tipo i enviadas por este processo.
	int num_processos;
	bool ativo;		//booleano para mecanismo de falha;
}args;


mensagem separar(char *str){
	int m[3];
	char* token;
	token = strtok(str,",");
	int i=0;
	while (token!= NULL){
	        m[i] = atoi(token);
	        token = strtok(NULL, ",");
	        i++;
	}
	mensagem msg;
	msg.tipo = m[0];
	msg.origem = m[1];
	msg.lider = m[2];
	return msg;
}

char* criar(mensagem msg, char strr[]){
	int m[3];
	m[0] = msg.tipo;
	m[1] = msg.origem;
	m[2] = msg.lider;

	//const int SIZE =11;
    char *strc = "0000"; //soma ao final da msg
	char *str; // parametro para retornar ponteiro
	//char strr[SIZE]; //buffer de mensagem
	int check; //inteiro para checar funcao snprintf

	check = snprintf(strr, 11, "%i,%i,%i,%s", m[0],m[1],m[2],strc);
	str = strr;
	return str;
}



void* interface(void* argum){ // thread de interface com usuario, recebe a estrutura compartilhada entre threads.
	args* argums = (args *)argum;
	char c;
	mensagem msg;
	char *buf;
    int sockfd, n;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr;
    socklen_t addrlen = sizeof(servaddr);

	printf("|       Digite uma das operaçoes:\n");
        printf("|       MENU :\n");
        printf("|               1 - Verificar se o lider esta ativo.\n");
        printf("|               2 - Falhar este processo.\n");
        printf("|               3 - Recuperar da falha.\n");
        printf("|               4 - Gerar estatisticas.\n");
        printf("|               0 - SAIR.\n");
        printf("|\n");

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(3000 + argums->id_lider);
    servaddr.sin_addr.s_addr = INADDR_ANY;

	while ((c = getchar())!= 48){
		printf("\nTecla: %c\n", c);
		if(c == 49){
			if (argums->ativo){
				printf("Enviando mensagem ao lider:");
				msg.tipo = 4; // mensagem VIVO
				msg.origem = argums->id_processo;
				msg.lider = argums->id_lider;
				buf = criar(msg, buffer);
                sendto(sockfd, buf, strlen(buf), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
                argums->num_msg[4]++;
			}
		}
		if(c==50){
			//sem_wait(&mutexAtivo);
			argums->ativo = false;
			//sem_post(&mutexAtivo);
			printf("O processo %i esta em falha\n", argums->id_processo);
		}
		if(c==51){
			//sem_wait(&mutexAtivo);
			argums->ativo = true;
			//sem_post(&mutexAtivo);
			printf("O processo %i esta operacional novamente\n", argums->id_processo);
		}
		if(c==52){
			printf("|	Estatisticas:\n");
			printf("|	Lider atual: Processo %i.\n", argums->id_lider);
			printf("|	Numero de mensagens enviadas:\n");
			printf("|	Total: %i\n", argums->num_msg[0]);
			printf("|	ELEICAO: %i\n", argums->num_msg[1]);
			printf("|	OK: %i\n", argums->num_msg[2]);
			printf("|	LIDER: %i\n", argums->num_msg[3]);
			printf("|	VIVO: %i\n", argums->num_msg[4]);
			printf("|	VIVO_OK: %i\n", argums->num_msg[5]);
			printf("|\n");

		}
		getchar();
	}
}


void* handler(void* argum) {
    args* argums = (args *)argum;
    mensagem msg;
    char *buf;
    int sockfd, recvlen;
    char buffer[MAXLINE];
    struct sockaddr_in clientAddr;
    struct sockaddr_in serverAddr;
    socklen_t addrlen = sizeof(clientAddr);
    struct ip_mreq mreq;
    struct sockaddr_in groupSock;
    struct in_addr localInterface;
    struct timeval tv;


    tv.tv_sec = 15;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

    memset(&clientAddr, 0, sizeof(clientAddr));
    memset(&serverAddr, 0, sizeof(serverAddr));
    memset((char *) &groupSock, 0, sizeof(groupSock));

    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(4000);
    serverAddr.sin_family = AF_INET;

    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = INADDR_ANY;
    groupSock.sin_port = htons(4000);

    int loopch = 0;
    setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch));

    localInterface.s_addr = inet_addr("192.168.1.66");

    bind(sockfd, (struct sockaddr*)&groupSock, sizeof(groupSock));

    mreq.imr_multiaddr.s_addr = inet_addr("225.1.1.1");
    mreq.imr_interface.s_addr = inet_addr("192.168.1.66");

    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    while(1){
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
        recvlen = recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *)&clientAddr, &addrlen);
        if(recvlen == -1){
            argums->id_lider = argums->id_processo;
            msg.origem = argums->id_processo;
            msg.lider = argums->id_processo;
            msg.tipo = 3;
            buf = criar(msg, buffer);
            setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface));
            sendto(sockfd, buf, strlen(buf), 0, (const struct sockaddr*)&groupSock, sizeof(groupSock));
            setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, INADDR_ANY, sizeof(INADDR_ANY));
            argums->num_msg[3]++;
            recvlen = 0;
        }

        buffer[recvlen] = '\0';
        buf = buffer;
        printf("%s \n", buf);
        msg = separar(buf);
        //printf("%i \n", msg.origem);
        if(msg.tipo == 1 && argums->ativo){
            if(msg.origem < argums->id_processo){
                msg.tipo = 2;
                msg.origem = argums->id_processo;
                msg.lider = argums->id_processo;
                buf = criar(msg, buffer);
                sendto(sockfd, buf, MAXLINE, 0, (struct sockaddr *)&clientAddr, addrlen);
                argums->num_msg[2]++;
                msg.tipo = 1;
                buf = criar(msg, buffer);
                setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface));
                sendto(sockfd, buf, strlen(buf), 0, (const struct sockaddr*)&groupSock, sizeof(groupSock));
                setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, INADDR_ANY, sizeof(INADDR_ANY));
                argums->num_msg[1]++;
            } else {}

        } else if(msg.tipo == 2 && argums->ativo){
                msg.origem = argums->id_processo;
        } else if(msg.tipo == 3){
                sem_wait(&mutexLider);
                argums->id_lider = msg.origem;
                sem_post(&mutexLider);
        } else if(msg.tipo == 4 && argums->ativo){
                msg.tipo = 5;
                msg.origem = argums->id_processo;
                msg.lider = argums->id_processo;
                buf = criar(msg, buffer);
                sendto(sockfd, buf, MAXLINE, MSG_WAITALL, (struct sockaddr *)&clientAddr, addrlen);
                argums->num_msg[5]++;
        }

    }

}

void* leaderAlive(void* argum) {
    args* argums = (args *)argum;
    mensagem msg;
    char *buf;
    int sockfd, n;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr;
    struct sockaddr_in groupSock;
    struct in_addr localInterface;
    socklen_t addrlen = sizeof(servaddr);
    struct timeval tv;

    tv.tv_sec = 5;


    // Creating socket file descriptor
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    memset((char *) &groupSock, 0, sizeof(groupSock));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(4000 + argums->id_processo);
    servaddr.sin_addr.s_addr = inet_addr("225.1.1.1");

    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = INADDR_ANY;
    groupSock.sin_port = htons(4000);

    int loopch = 0;
    setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch));

    localInterface.s_addr = inet_addr("225.1.1.1");

    while(1){
        sleep(3);
        msg.tipo = 4;
        msg.origem = argums->id_processo;
        msg.lider = argums->id_processo;
        buf = criar(msg, buffer);
        sendto(sockfd, buf, strlen(buf), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        argums->num_msg[4]++;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0, (struct sockaddr *) &servaddr, &addrlen);
        buffer[n] = '\0';
        buf = buffer;
        msg = separar(buf);
        //printf("%i \n", n);

        if(msg.tipo == 5 && (n != -1) && argums->ativo){}
         else {
            msg.tipo = 1;
            msg.lider = argums->id_processo;
            msg.origem = argums->id_processo;
            buf = criar(msg, buffer);
            //printf("%s \n", buf);
            setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface));
            sendto(sockfd, (const char *)buf, strlen(buf), MSG_CONFIRM, (const struct sockaddr*)&groupSock, sizeof(groupSock));
            setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, INADDR_ANY, sizeof(INADDR_ANY));
            argums->num_msg[1]++;

        }
    }

}



int main(int argc, char** argv) {

    args argumento;
    argumento.id_lider = 0;
    argumento.id_processo = atoi(argv[1]);
    argumento.num_processos = 2;
    argumento.ativo = true;
    for(int i = 0; i <= 6; i++){
        argumento.num_msg[i] = 0;
    }

    pthread_t tids[3];

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&tids[0], &attr, handler, &argumento);

    pthread_attr_init(&attr);
    pthread_create(&tids[1], &attr, leaderAlive, &argumento);

    pthread_attr_init(&attr);
    pthread_create(&tids[2], &attr, interface, &argumento);


    pthread_join(tids[0], NULL);
    pthread_join(tids[1], NULL);
    pthread_join(tids[2], NULL);




}
