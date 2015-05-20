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


typedef struct {
    char* nomeMem;
    int duracaoaberturabalcao;
} Desk_m;

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
	printf("N_BALCAO\tN_TEMPO\tNDURACAO\tNOMEFIFO\tEMATEND\tJAATEND\tTEMPMEDIO");

printf("\n");
int i = 0; 
	
	//creates the table
	while(i < shm->nrBalcoes){

		printf("%d\t\t%d\t\t%d\t%d\t\t%d\t%d\t%d\n",
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


//desk thread
void *thr_func(void *content){
	Desk_m * membalcao = (Desk_m*) content;
	
	Store_memory *shm = create_shared_memory(membalcao->nomeMem, sizeof(Store_memory));
	
	char b_fifoname[200] = "/tmp/fb_";
	char pid[50];
	sprintf(pid, "%d", getpid());  //gets process id 
	strcat(b_fifoname, pid);	//completes name of the fifo with the pid
	mkfifo(b_fifoname, 0660);	//creates the fifo

	int f_name = open(b_fifoname, O_RDONLY | O_NONBLOCK);

//PRINTS PARA APAGAR
	printf("Pid nr:%d\n", getpid()); 
	printf("Nome fifo balcao: %s\n", b_fifoname);

	printf("Name mem: %s\n", membalcao->nomeMem);
	printf("Duracao do balcao: %d\n", membalcao->duracaoaberturabalcao);
	printf("Horas abertura da loja:");
	printf("%s",ctime(&shm->tempoaberturaloja));
	printf("Nr Balcoes: %d\n", shm->nrBalcoes);
//-------------------

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

	

	while(elapsed_time < membalcao->duracaoaberturabalcao){

		//printf("Segundos restantes: %d\n", elapsed_time);
		char str[100] = "";

		if(readLine(f_name, str)){
			printf("Nome do cliente a ler %s\n", str);
            if(shm->table[NR_ATENDIMENTO][blc] <= 10)
            	sleep(shm->table[NR_ATENDIMENTO][blc]);
            else
            	sleep(10);

            mkfifo(str, 0660);

            int e_msg = open(str, O_WRONLY);
            char endmsg[] = "fim_atendimento";
            shm->table[NR_ATENDIMENTO][blc] = shm->table[NR_ATENDIMENTO][blc] - 1;
	    shm->table[NR_JATEND][blc] = shm->table[NR_JATEND][blc] + 1;
            write(e_msg, endmsg, sizeof(endmsg));

            close(e_msg);
        }

		elapsed_time = time(NULL)-start;

	}

	   int atendidostotal = shm->table[NR_JATEND][blc];
           int duracaob = membalcao->duracaoaberturabalcao;
	   int tmedio = duracaob/atendidostotal;
	   shm->table[TEMPOMEDIO][blc] = tmedio;


	shm->table[NR_DURACAO][blc] = membalcao->duracaoaberturabalcao;
	printf("Balcoes tempo: %d", membalcao->duracaoaberturabalcao);

	if(shm->nrBalcoesAbertos == 1)
		destroy_shared_memory(shm, sizeof(Store_memory), membalcao->nomeMem);
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

	
}

