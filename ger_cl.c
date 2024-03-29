#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
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
	pthread_mutex_t mutex; 

	int buffer[BUFSIZE];

	FILE * log_file;
	char log_name[200];
	int nrBalcoes;
	int nrBalcoesAbertos;
	time_t tempoaberturaloja;
	double table[7][500];

} Store_memory;

void writeLogEntry(Store_memory* shm, int nr, char* event, int current_pid){ //function to facilitate the writing in the log file of all information

	shm->log_file = fopen(shm->log_name, "a"); //opens log file

	// gets current time of the event
	time_t current_time = time(NULL);
	struct tm* tm_info;
	char buffer[26];
	tm_info = localtime(&current_time);
	strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
	//---------------

	//writes the channel information
	char channel[200]; 
	if(current_pid != 0){
		sprintf(channel, "fc_%d", current_pid);
	}else sprintf(channel , "-");


	fprintf(shm->log_file, "%-22s | Cliente  | %-5d  | %-22s | %-10s \n", buffer, nr, event, channel); //writes on log file, when the event occurs, where, in which desk, what was the event and which channel was used

	fclose(shm->log_file); //closes file

}

//reads a line from the fifo
int readLine(int fd, char *str){

	int n;

	do{
		n = read(fd,str,1);
	}while(n>0 && *str++ != '\0');


	return (n>0);
}


//GETS SHARED MEMORY
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


//Gets best desk for the current client
int melhorbalcao(Store_memory *shm){ 

	int  i = 0;
	int n = -1;
	int minimo; 

	while(pthread_mutex_trylock(&shm->mutex))
	{
		continue;

	}	
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
	pthread_mutex_unlock(&shm->mutex);

	while(pthread_mutex_trylock(&shm->mutex))
	{
		continue;

	}	
	shm->table[NR_ATENDIMENTO][n] = shm->table[NR_ATENDIMENTO][n] + 1;  //updates the number of people in line of the desk chosen
	pthread_mutex_unlock(&shm->mutex);

	return n;  //returns the best desk number

}

int main(int argc, char *argv[]){

	
	//checks the arguments
	if(argc != 3){
		printf("\nWrong number of arguments\n");
		return 1;
	}
	
	//gets the information passed through the arguments
	int nr_clientes = atoi(argv[2]); 
	Store_memory *shm;
	shm = get_shared_memory(argv[1], sizeof(Store_memory));
	//-----

	int i = 0;

	while(i < nr_clientes){  //while there is still clients to attend
		pid_t pid = fork();
		

		if(pid < 0){		//checks for errors on fork
			perror("Error in fork");
			return 1;
		}
		else if (pid == 0){ //if child then starts the events

			//creates the channel for the client
			char c_fifoname[200] = "/tmp/fc_";
			char pid[50];
			sprintf(pid, "%d", getpid());  
			int cpid = getpid();
			strcat(c_fifoname, pid);
			mkfifo(c_fifoname, 0660);


			int indicebalcao = melhorbalcao(shm);  //gets best desk
			char bestb_fifoname[200] = "/tmp/fb_";
			char pidb[60];

			writeLogEntry(shm, indicebalcao+1, "pede_atendimento", cpid); //writes on log file the event "pede atendimento;

			//the client starts requesting being attended on the desk chosen

			while(pthread_mutex_trylock(&shm->mutex))
			{
				continue;
				
			}	

			sprintf(pidb, "%d", (int) shm->table[NM_FIFO][indicebalcao]);  

			pthread_mutex_unlock(&shm->mutex);


			strcat(bestb_fifoname, pidb);
			mkfifo(bestb_fifoname, 0660);
			

			int bestb_name = open(bestb_fifoname, O_WRONLY); 


			int length = strlen(c_fifoname) + 1;
			write(bestb_name, c_fifoname, length);  

			int fc_name = open(c_fifoname, O_RDONLY);
			char str[100];
			

			while(readLine(fc_name, str)){ //keeps reading 

				if(strcmp(str,"fim_atendimento") == 0) //if the client receives the msg "fim atendimento" it means that he has been attended
					{	writeLogEntry(shm, indicebalcao+1, "fim_atendimento", getpid()); //writes on log file the event "fim atendimento";
			}else{ //if the client never receives the msg that means something is wrong
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
