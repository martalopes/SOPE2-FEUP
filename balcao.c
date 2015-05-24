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
	pthread_mutex_t mutex; 

	int buffer[BUFSIZE]; 

	FILE * log_file;
	char log_name[200];

	int nrBalcoes;
	int nrBalcoesAbertos;
	time_t tempoaberturaloja;
	double table[7][500];

} Store_memory; 	//shared memory


typedef struct {
	char* nomeMem;
	int duracaoaberturabalcao;
} Desk_m;			//desk information


typedef struct {
	Store_memory* nomeMem;
	int balcao_nr;
	char str[300];
} argsatendimento;	//information for the treatment of the clients

void fileInit(Store_memory* shm){ //inicializes log file

	shm->log_file = fopen(shm->log_name, "w");	//sets log file name

	// Writes the beggining of the table on the log file to organize the information
	fprintf(shm->log_file, "Ficheiro log \n\n");		
	fprintf(shm->log_file, " quando                | quem     | balcao |  o_que                 | canal_criado/usado\n");
	fprintf(shm->log_file, "----------------------------------------------------------------------------------------\n");
	//------------------------------------------------------------------

	fclose(shm->log_file);	//closes file
}

void writeLogEntryCharPid(FILE* file, char* filename, int nr_balcao, char* event, char* current_pid){  //function to facilitate the writing in the log file of all information

	file = fopen(filename, "a");  //opens log file

	// gets current time of the event
	time_t current_time = time(NULL);  
	struct tm* tm_info;	
	char buffer[26];
	tm_info = localtime(&current_time); 
	strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
	//---------------

	fprintf(file, "%-22s | Balcao   | %-5d  | %-23s| %-10s \n", buffer, nr_balcao, event, current_pid); //writes on log file, when the event occurs, where, in which desk, what was the event and which channel was used

	fclose(file); //closes file

}


//CREATES THE MEMORY
Store_memory * create_shared_memory(char * shm_name, int shm_size) 
{ 
	int shmfd; 
	int m_exst = 0;
	Store_memory *shm; 

	//create the shared memory region 
	shmfd = shm_open(shm_name,O_CREAT|O_RDWR|O_EXCL,0660); 

	if(shmfd<0){ 
		shmfd = shm_open(shm_name,O_RDWR,0660);


		if(shmfd <= 0){ //if the memory does not exist but it fails
			m_exst = -1;
			perror("ERROR in shm_open()");
			return NULL; 
		}
		else //if the memory exists
			m_exst = 1;
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

	//if the memory does not exists
	if(m_exst == 0){
		shm->tempoaberturaloja = time(NULL); //opening time of the store is set to current
		shm->nrBalcoes = 1;	//inicializes the number of desks 
		shm->nrBalcoesAbertos = 1; //inicializes the number of open desks
		
		char* log_name = malloc(sizeof(char) * 200);
		strcpy(log_name, shm_name);
		log_name++;
		strcat(log_name, ".log");
		strcpy (shm->log_name, log_name);
		
		pthread_mutex_init(&shm->mutex, NULL); 

		log_name--;
		free(log_name); //frees memory

		fileInit(shm);
		writeLogEntryCharPid(shm->log_file, shm->log_name, 1, " inicia_mempart", "0"); //writes on log file the event "inicia memoria partilhada";

	}
	//if the memory already exists
	else if(m_exst == 1){
		shm->nrBalcoes++; //adds one more desk
		shm->nrBalcoesAbertos++; //adds one more open desk
	}

	return (Store_memory *) shm; 
} 

//DESTROYS THE MEMORY 
void destroy_shared_memory(Store_memory *shm, int shm_size, char * nomeMem)
{ 

	printf("\n\n\n\n\n\n\n");
	printf("N_B\t\tT\t\tDUR\t\tFIFO\t\tEM_AT\t\tJA_AT\t\tTM");

	printf("\n");
	int i = 0; 
	
	//creates the table
	while(i < shm->nrBalcoes){

		printf("%f\t%f\t%f\t%f\t%f\t%f\t%f\n",
			shm->table[NR_BALCAO][i],
			shm->table[NR_TEMPO][i],
			shm->table[NR_DURACAO][i],
			shm->table[NM_FIFO][i],
			shm->table[NR_ATENDIMENTO][i],
			shm->table[NR_JATEND][i],
			shm->table[TEMPOMEDIO][i]);
		

		i++;
	}

	//CHECKS FOR ERRORS
	if(munmap(shm,shm_size) < 0){  
		perror("Failure in munmap()"); 
		exit(EXIT_FAILURE); 
	} 

	if(shm_unlink(nomeMem) < 0){ 
		perror("Failure in shm_unlink()"); 
		exit(EXIT_FAILURE); 
	} 
	//-----------------
} 

int readLine(int fd, char *str){ //on fifo

	int n;

	do{
		n = read(fd,str,1);
	}while(n>0 && *str++ != '\0');


	return (n>0);
}


//THREAD USED FOR TREATING ALL CLIENTS
void *thr_atendimento(void *args){
	char str[300];
	Store_memory * shm = ((argsatendimento *) args)->nomeMem;  //gets the name of the memory that is going to be used
	int blc = ((argsatendimento *) args)->balcao_nr;		  //gets the number of the desk where the client is going to be send
	strcpy(str, ((argsatendimento *) args)->str);			//gets information about the client

	//Gets channel used in this event for writing to log file
	char* newstr = malloc(sizeof(char) * 50);  
	char* newstr2 = malloc(sizeof(char) * 50);
	strcpy(newstr, str);
	newstr = newstr + 5;
	strcpy(newstr2, newstr);
	//------------------------

	writeLogEntryCharPid(shm->log_file, shm->log_name, blc+1, "inicia_atend_cli", newstr); //writes on log file the event "inicia atendimento do cliente";

	newstr = newstr -5;
	free(newstr); //frees memory

	while(pthread_mutex_trylock(&shm->mutex))  
	{
		continue;

	}	

	double fila = shm->table[NR_ATENDIMENTO][blc];  //gets number of people in line

	pthread_mutex_unlock(&shm->mutex);
	
	//makes the client wait x seconds depending on the number of people in line but never more then 10 secs
	if(fila <= 10)  
		sleep(fila + 1);
	else
		sleep(10);
	

	mkfifo(str, 0660);


	int e_msg = open(str, O_WRONLY);  
	char endmsg[] = "fim_atendimento";  //creates the msg to send the client that he has been attended 
	

	
	while(pthread_mutex_trylock(&shm->mutex))
	{
		continue;

	}	
	//updates the number of people in line and people already treated
	shm->table[NR_ATENDIMENTO][blc]--;  
	shm->table[NR_JATEND][blc]++;
	//-----------------

	pthread_mutex_unlock(&shm->mutex);
	
	writeLogEntryCharPid(shm->log_file, shm->log_name, blc+1, "fim_atend_cli", newstr2); //writes on log file the event "fim atendimento do cliente";

	free(newstr2); //frees memory


	write(e_msg, endmsg, sizeof(endmsg)); //sends message to the client 
	

	close(e_msg);

	pthread_exit(NULL);
}

//DESK THREAD
void *thr_func(void *content){
	Desk_m * membalcao = (Desk_m*) content;
	
	Store_memory *shm = create_shared_memory(membalcao->nomeMem, sizeof(Store_memory));


	char b_fifoname[200] = "/tmp/fb_";
	char pid[50];
	sprintf(pid, "%d", getpid());  //gets process id 
	strcat(b_fifoname, pid);	//completes name of the fifo with the pid
	mkfifo(b_fifoname, 0660);	//creates the fifo

	printf("Balcao nr %d aberto em: %s", shm->nrBalcoes, ctime(&shm->tempoaberturaloja));
	shm->table[NR_BALCAO][shm->nrBalcoes-1] = shm->nrBalcoes;
	shm->table[NR_TEMPO][shm->nrBalcoes-1] = time(NULL) - shm->tempoaberturaloja;
	shm->table[NR_DURACAO][shm->nrBalcoes-1] = -1; //-1 while it's open, when the desk closes it's changed
	shm->table[NM_FIFO][shm->nrBalcoes-1] = getpid();
	shm->table[NR_ATENDIMENTO][shm->nrBalcoes-1] = 0; //starts counting the number of clients being assisted
	shm->table[NR_JATEND][shm->nrBalcoes-1]= 0;		//starts counting the number of clients already assisted
	shm->table[TEMPOMEDIO][shm->nrBalcoes-1]= 0;	//starts counting the average assistance time

	int f_name = open(b_fifoname, O_RDONLY | O_NONBLOCK);
	int blc = shm->nrBalcoes -1 ;
	

	//Gets the channel being used in the events for writing on log file
	char pidchar[300];
	char nrpid[300];
	strcpy(pidchar, "fb_");
	sprintf(nrpid, "%d", getpid() );
	strcat(pidchar, nrpid);
	//-----------------------


	writeLogEntryCharPid(shm->log_file, shm->log_name, blc+1, "cria_linh_mempart", pidchar); //writes on log file the event "cria linha da memoria partilhada";



	pthread_t atendimento;
	int start = time(NULL);
	int elapsed_time = time(NULL) - start;

	while(elapsed_time < membalcao->duracaoaberturabalcao){  //while the desk is still open
		
		
		char str[100] = "";

		if(readLine(f_name, str)){			
			
			argsatendimento * args = malloc(sizeof(argsatendimento));
			args->nomeMem = shm;
			args->balcao_nr = blc;
			strcpy(args->str, str); //gets the client 

			
			pthread_create(&atendimento, NULL, thr_atendimento, (void*) args); //sends client to thr_atendimento 
			

		}

		elapsed_time = time(NULL)-start; //updates elapsed time
	}

	
	time_t opening_time = time(NULL);
	while(shm->table[NR_ATENDIMENTO][blc]){
		pthread_join(atendimento, NULL); //joins all the threads of the clients and waits for all of them to finish
		if(time(NULL) - opening_time > 30) shm->table[NR_ATENDIMENTO][blc]--; //in case that the program crashes waits 30 seconds and then ignores that action
	}

	//updates info
	double atendidostotal = shm->table[NR_JATEND][blc];    
	double duracaob = membalcao->duracaoaberturabalcao;
	double tmedio = duracaob/atendidostotal;
	shm->table[TEMPOMEDIO][blc] = tmedio;
	shm->table[NR_DURACAO][blc] = membalcao->duracaoaberturabalcao;
	//---
	printf("Duracao do balcao: %d", membalcao->duracaoaberturabalcao);

	char pidchar2[300];
	char nrpid2[300];
	strcpy(pidchar2, "fb_");
	sprintf(nrpid2, "%d", getpid() );
	strcat(pidchar2, nrpid2);



	writeLogEntryCharPid(shm->log_file, shm->log_name, blc+1, "fecha_balcao", pidchar2); //writes on log file the event "fecha balcao";


	if(shm->nrBalcoesAbertos == 1){ //if the last desk is closing
		writeLogEntryCharPid(shm->log_file, shm->log_name, blc + 1, "fecha_loja ", pidchar2); //writes on log file the event "fecha loja";
		destroy_shared_memory(shm, sizeof(Store_memory), membalcao->nomeMem); //the store will close too
	}

	else
		shm->nrBalcoesAbertos--; //else it justs updates the number of desks open


	free(membalcao); //frees the memory
	close(f_name);
	pthread_exit(NULL); //closes the thread


}

int main(int nrarg, char *path[]){ 


	if(nrarg != 3){ //checks if the number of arguments is right
		printf("\nWrong number of arguments\n");
		return 1;
	}

	Desk_m *sending;
	sending = (Desk_m *) malloc(sizeof(Desk_m));   

	//sends information about the desk to the struct
	sending->duracaoaberturabalcao = atoi(path[2]);
	sending->nomeMem = path[1];  
	//--------------

	pthread_t tid;
	pthread_create(&tid, NULL, thr_func, (void*) sending);  //creates the desk thread

	pthread_exit(NULL);

	return 0;

	
}

