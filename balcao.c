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

} Store_memory; 


typedef struct {
	char* nomeMem;
	int duracaoaberturabalcao;
} Desk_m;


typedef struct {
	Store_memory* nomeMem;
	int balcao_nr;
	char str[300];
} argsatendimento;

void fileInit(Store_memory* shm){

	shm->log_file = fopen(shm->log_name, "w");

	fprintf(shm->log_file, "Ficheiro log \n\n");
	fprintf(shm->log_file, " quando\t\t    | quem\t | balcao |  o_que\t\t| canal_criado/usado\n");
	fprintf(shm->log_file, "----------------------------------------------------------------------------------------\n");

	fclose(shm->log_file);

}

/*void writeLogEntry(Store_memory* shm, int nr_balcao, char* event, int current_pid){

	shm->log_file = fopen(shm->log_name, "a");

	time_t current_time = time(NULL);
	struct tm* tm_info;
	char buffer[26];
    tm_info = localtime(&current_time);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    char channel[200];
    if(current_pid != 0){
    sprintf(channel, "fb_%d", current_pid);
	}else sprintf(channel , "-");

	fprintf(shm->log_file, "%s | Balcao\t | %d\t  | %s\t| %s\n", buffer, nr_balcao, event, channel);

	fclose(shm->log_file);

}*/

	void writeLogEntryCharPid(FILE* file, char* filename, int nr_balcao, char* event, char* current_pid){

		file = fopen(filename, "a");

		time_t current_time = time(NULL);
		struct tm* tm_info;
		char buffer[26];
		tm_info = localtime(&current_time);
		strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

		fprintf(file, "%s | Balcao\t | %d\t  | %s\t| %s\n", buffer, nr_balcao, event, current_pid);

		fclose(file);

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
		free(log_name);

		fileInit(shm);
		writeLogEntryCharPid(shm->log_file, shm->log_name, 1, " inicia_mempart", "0");

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


	if(munmap(shm,shm_size) < 0){ 
		perror("Failure in munmap()"); 
		exit(EXIT_FAILURE); 
	} 

	if(shm_unlink(nomeMem) < 0){ 
		perror("Failure in shm_unlink()"); 
		exit(EXIT_FAILURE); 
	} 
} 

int readLine(int fd, char *str){ //on fifo

	int n;

	do{
		n = read(fd,str,1);
	}while(n>0 && *str++ != '\0');


	return (n>0);
}

void *thr_atendimento(void *args){
	char str[300];
	Store_memory * shm = ((argsatendimento *) args)->nomeMem;
	int blc = ((argsatendimento *) args)->balcao_nr;
	strcpy(str, ((argsatendimento *) args)->str);

	char* newstr = malloc(sizeof(char) * 50);
	char* newstr2 = malloc(sizeof(char) * 50);
	strcpy(newstr, str);
	newstr = newstr + 5;
	strcpy(newstr2, newstr);

	writeLogEntryCharPid(shm->log_file, shm->log_name, blc+1, "inicia_atend_cli", newstr);

	newstr = newstr -5;
	free(newstr);

	while(pthread_mutex_trylock(&shm->mutex))
	{
		continue;

	}	

	double fila = shm->table[NR_ATENDIMENTO][blc];
	pthread_mutex_unlock(&shm->mutex);
	

	if(fila <= 10)
		sleep(fila + 1);
	else
		sleep(10);
	

	mkfifo(str, 0660);


	int e_msg = open(str, O_WRONLY);
	char endmsg[] = "fim_atendimento";
	

	
	while(pthread_mutex_trylock(&shm->mutex))
	{
		continue;

	}	
	
	shm->table[NR_ATENDIMENTO][blc]--;
	shm->table[NR_JATEND][blc]++;

	pthread_mutex_unlock(&shm->mutex);
	
	writeLogEntryCharPid(shm->log_file, shm->log_name, blc+1, "fim_atend_cli", newstr2);

	free(newstr2);


	write(e_msg, endmsg, sizeof(endmsg));
	

	close(e_msg);
	pthread_exit(NULL);
}


//desk thread
void *thr_func(void *content){
	Desk_m * membalcao = (Desk_m*) content;
	
	Store_memory *shm = create_shared_memory(membalcao->nomeMem, sizeof(Store_memory));


	char b_fifoname[200] = "/tmp/fb_";
	char pid[50];
	sprintf(pid, "%d", getpid());  //gets process id 
	strcat(b_fifoname, pid);	//completes name of the fifo with the pid
	mkfifo(b_fifoname, 0660);	//creates the fifo

	shm->table[NR_BALCAO][shm->nrBalcoes-1] = shm->nrBalcoes;
	shm->table[NR_TEMPO][shm->nrBalcoes-1] = time(NULL) - shm->tempoaberturaloja;
	shm->table[NR_DURACAO][shm->nrBalcoes-1] = -1; //-1 while it's open, when the desk closes it's changed
	shm->table[NM_FIFO][shm->nrBalcoes-1] = getpid();
	shm->table[NR_ATENDIMENTO][shm->nrBalcoes-1] = 0; //starts counting the number of clients being assisted
	shm->table[NR_JATEND][shm->nrBalcoes-1]= 0;		//starts counting the number of clients already assisted
	shm->table[TEMPOMEDIO][shm->nrBalcoes-1]= 0;	//starts counting the average assistance time


	int blc = shm->nrBalcoes -1 ;
	int start = time(NULL);
	int elapsed_time = time(NULL) - start;

	char pidchar[300];
	char nrpid[300];
	strcpy(pidchar, "fb_");
	sprintf(nrpid, "%d", getpid() );
	strcat(pidchar, nrpid);


	writeLogEntryCharPid(shm->log_file, shm->log_name, blc+1, "cria_linh_mempart", pidchar);

	pthread_t atendimento;

	while(elapsed_time < membalcao->duracaoaberturabalcao){
		
		
		char str[100] = "";

		if(readLine(f_name, str)){
			
			argsatendimento * args = malloc(sizeof(argsatendimento));
			args->nomeMem = shm;
			args->balcao_nr = blc;
			strcpy(args->str, str);

			
			pthread_create(&atendimento, NULL, thr_atendimento, (void*) args); 
			

		}

		elapsed_time = time(NULL)-start;
	}

	
	time_t opening_time = time(NULL);
	while(shm->table[NR_ATENDIMENTO][blc]){
		pthread_join(atendimento, NULL);
		if(time(NULL) - opening_time > 30) shm->table[NR_ATENDIMENTO][blc]--;
	}

	double atendidostotal = shm->table[NR_JATEND][blc];
	double duracaob = membalcao->duracaoaberturabalcao;
	double tmedio = duracaob/atendidostotal;
	shm->table[TEMPOMEDIO][blc] = tmedio;


	shm->table[NR_DURACAO][blc] = membalcao->duracaoaberturabalcao;
	printf("Balcoes tempo: %d", membalcao->duracaoaberturabalcao);

	char pidchar2[300];
	char nrpid2[300];
	strcpy(pidchar2, "fb_");
	sprintf(nrpid2, "%d", getpid() );
	strcat(pidchar2, nrpid2);



	writeLogEntryCharPid(shm->log_file, shm->log_name, blc+1, "fecha_balcao", pidchar2);


	if(shm->nrBalcoesAbertos == 1){
		writeLogEntryCharPid(shm->log_file, shm->log_name, blc + 1, "fecha_loja \t", pidchar2);
		destroy_shared_memory(shm, sizeof(Store_memory), membalcao->nomeMem);
	}

	else
		shm->nrBalcoesAbertos--;


	free(membalcao); //frees the memory
	close(f_name);
	pthread_exit(NULL); //closes the thread


}

int main(int nrarg, char *path[]){ 


	if(nrarg != 3){
		printf("\nWrong number of arguments\n");
		return 1;
	}

	Desk_m *sending;
	sending = (Desk_m *) malloc(sizeof(Desk_m));

	sending->duracaoaberturabalcao = atoi(path[2]);
	sending->nomeMem = path[1];

	pthread_t tid;
	pthread_create(&tid, NULL, thr_func, (void*) sending); 

	pthread_exit(NULL);

	return 0;

	
}

