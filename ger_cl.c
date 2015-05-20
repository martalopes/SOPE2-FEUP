#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>

#define BUFSIZE 5
#define NUMITEMS 50
#define NR_BALCAO 0
#define NR_TEMPO 1
#define	NR_DURACAO 2
#define NM_FIFO 3
#define NR_ATENDIMENTO 4
#define NR_JATEND 5
#define TEMPOMEDIO 6

typedef struct { 
	pthread_mutex_t buffer_lock;
	pthread_cond_t  slots_cond;
	pthread_cond_t  items_cond;
	pthread_mutex_t slots_lock;
	pthread_mutex_t items_lock;

	int buffer[BUFSIZE];

	int nrBalcoes;
	int nrBalcoesAbertos;
	time_t tempoaberturaloja;
	int table[7][500];

} Store_memory;

//reads a line from the fifo
int readLine(int fd, char *str){

	int n;

	do{
		n = read(fd,str,1);
	}while(n>0 && *str++ != '\0');


	return (n>0);
}

Store_memory * get_shared_memory(char * shm_name, int shm_size) 
{ 
	int shmfd; 
	Store_memory *shm; 


	//create the shared memory region 
	shmfd = shm_open(shm_name,O_RDWR,0660); 

	if(shmfd <= 0){
			perror("ERROR in shm_open()");
			exit(EXIT_FAILURE); 
	}
	
	//specify the size of the shared memory region 
	if (ftruncate(shmfd,shm_size) < 0){ 
		perror("Failure in ftruncate()"); 
		return NULL; 
	} 

	//attach this region to virtual memory 
	shm = mmap(0,shm_size,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0); 

	if(shm == MAP_FAILED){ 
		perror("Failure in mmap()"); 
		return NULL; 
	} 

	return (Store_memory *) shm; 
} 

int melhorbalcao(Store_memory *shm){

	int  i = 0;
	int n = -1;
	int minimo; 

	while(i < shm->nrBalcoesAbertos){

		if(n == -1){
			n = i;
			minimo = shm->table[NR_ATENDIMENTO][i];
		}
		else if(minimo > shm->table[NR_ATENDIMENTO][i])
		{
			n = i; 
			minimo = shm->table[NR_ATENDIMENTO][i];
		}
		i++;
	}

	shm->table[NR_ATENDIMENTO][n] = shm->table[NR_ATENDIMENTO][n] + 1;
	return n;

}

int main(int argc, char *argv[]){

																		setbuf(stdout, NULL);
	//checks the arguments
	if(argc != 3){
		printf("\nWrong number of arguments\n");
		return 1;
	}


	int nr_clientes = atoi(argv[2]);
	Store_memory *shm;
	shm = get_shared_memory(argv[1], sizeof(Store_memory));
	int i = 0;

	while(i < nr_clientes){
		pid_t pid = fork();

		if(pid < 0){
			perror("Error in fork");
			return 1;
		}
		else if (pid == 0){

			char c_fifoname[200] = "/tmp/fc_";
			char pid[50];
			sprintf(pid, "%d", getpid());  
			strcat(c_fifoname, pid);
			mkfifo(c_fifoname, 0660);


			int indicebalcao = melhorbalcao(shm);
			char bestb_fifoname[200] = "/tmp/fb_";
			char pidb[60];
			sprintf(pidb, "%d", shm->table[NM_FIFO][indicebalcao]);
			strcat(bestb_fifoname, pidb);
			mkfifo(bestb_fifoname, 0660);
			

			int bestb_name = open(bestb_fifoname, O_WRONLY);


			int length = strlen(c_fifoname) + 1;
			write(bestb_name, c_fifoname, length);
			printf("No cliente o nome do fifo é %s", c_fifoname);

			int fc_name = open(c_fifoname, O_RDONLY);
			char str[100];

			while(readLine(fc_name, str)){

				if(strcmp(str,"fim_atendimento") == 0)
				{
					printf("O cliente com pid %d foi notificado do fim de atendimento\n", getpid());
					shm->table[NR_ATENDIMENTO][indicebalcao] = shm->table[NR_ATENDIMENTO][indicebalcao] - 1;
					shm->table[NR_JATEND][indicebalcao] = shm->table[NR_JATEND][indicebalcao] +1;

				}else{
					printf("ERRO! Nao recebeu a notificacao de fim de atendimento\n");
					exit(EXIT_FAILURE);
				}

			}


			close(fc_name);
			close(bestb_name);
			exit(EXIT_SUCCESS);
			
		}

		i++;
	}

	return 0;

}
