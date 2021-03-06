///////////////////Libraries///////////////////////

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include <sys/mman.h> //Library for mmap
#include <pthread.h>
#include <sys/stat.h> //Library for struct stat
#include <sys/sysinfo.h> // Library for get_nprocs()
#include <unistd.h>

///////////////////////////////////////////////////

/////////////////GLOBAL VARIABLES////////////////////////

int total_threads; //Total number of threads that will be created for consumer.
int page_size; //Page size = 4096 Bytes
int num_files; //Total number of the files passed as the arguments.
int isComplete=0; //Flag needed to wakeup any sleeping threads at the end of program.
int total_pages; //required for the compressed output
int q_head=0; //Circular queue head.
int q_tail=0; //Circular queue tail.
#define q_capacity 10  
int q_size=0; //Circular queue capacity.
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER, filelock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER, fill = PTHREAD_COND_INITIALIZER;
int* pages_per_file;

/////////////////////////////////////////////////////////

/////////////////STRUCTURES///////////////////////////////

//Contains the compressed output

struct output {
	char* data;
	int* count;
	int size;
}*out;

//Contains page specific data of a specific file.

struct buffer {
    char* address; //Mapping address of the file_number file + page_number page
    int file_number; //File Number
    int page_number; //Page number
    int last_page_size; //Page sized or (size_of_file)%page_size
}buf[q_capacity];

//Contains file specific data for munmap

struct fd{
	char* addr;
	int size;
}*files;

////////////////////////////////////////////////////////

/////////////////QUEUE Functions////////////////////////

//Add at q_head index of the circular queue. 

void put(struct buffer b){
  	buf[q_head] = b; //Enqueue the buffer
  	q_head = (q_head + 1) % q_capacity;
  	q_size++;
}

//Remove from q_tail index of the circular queue.

struct buffer get(){
  	struct buffer b = buf[q_tail]; //Dequeue the buffer.
	q_tail = (q_tail + 1) % q_capacity;
  	q_size--;
  	return b;
}

////////////////////////////////////////////////////////

////////////////////////PRODUCER/////////////////////////

//Producer function to memory map the files passed as arguments.

void* producer(void *arg){
	
	//Step 1: Get the file.
	
	char** filenames = (char **)arg;
	struct stat sb;
	char* map; 				//mmap address
	int file;
	
	//Step 2: Open the file
	
	for(int i=0;i<num_files;i++){
		
		file = open(filenames[i], O_RDONLY);
		int pages_in_file=0; 			//Calculates the number of pages in the file. Number of pages = Size of file / Page size.
		int last_page_size=0; 			//If the file is not page-alligned 
		
		if(file == -1){ 			// If file not found?
			
			printf("Error: File didn't open\n");
			exit(1);
		}

		//Step 3: Get the file info.
		
		if(fstat(file,&sb) == -1){
			
			close(file);
			printf("Error: Couldn't retrieve file stats");
			exit(1);
		}
		
		
        	if(sb.st_size==0){			//Empty files 
               		continue;
        	}
		
		//Step 4: Calculate the number of pages and last page size.
		
		
		pages_in_file=(sb.st_size/page_size);		//st_size contains the size offset. 
		
		if(((double)sb.st_size/page_size)>pages_in_file){ 	//If the file is not page alligned, we will add an extra page.
			
			pages_in_file+=1;
			last_page_size=sb.st_size%page_size;
		}
		
		else{ 							//Page alligned
			last_page_size=page_size;
		}
		
		total_pages+=pages_in_file;
		pages_per_file[i]=pages_in_file;
		
		
		//Step 5: Map the entire file.
		
		map = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, file, 0); 			//If addr is NULL, then the kernel chooses the address
																	  
		if (map == MAP_FAILED) { 
			
			close(file);
			printf("Error mmapping the file\n");
			exit(1);
    	}	
    	
    	       //Step 6: For all the pages in file create a Buffer.
		
		for(int j=0;j<pages_in_file;j++){
			
			pthread_mutex_lock(&lock);
			while(q_size==q_capacity){
			    pthread_cond_broadcast(&fill); 				//Wake up all the sleeping consumer threads.
				pthread_cond_wait(&empty,&lock); 			//Call the consumer to start working on the queue.
			}
			pthread_mutex_unlock(&lock);
			
			struct buffer temp;
			
			if(j==pages_in_file-1){ 					//Check if last page
				
				temp.last_page_size=last_page_size;
			}
			
			else{
				temp.last_page_size=page_size;
			}
			
			temp.address=map;
			temp.page_number=j;
			temp.file_number=i;
			map+=page_size;							//Go to next page in the memory.
			
			pthread_mutex_lock(&lock);				       //possible race condition while changing size.
			put(temp);
			pthread_mutex_unlock(&lock);
			pthread_cond_signal(&fill);
		}
		
		//Step 7: Close the file.
		
		close(file);
	}
	
	isComplete=1;									 //When producer is done mapping.
	pthread_cond_broadcast(&fill); 							//Wake-up all the sleeping consumer threads.
	
	return 0;
}
/////////////////////////////////////////////////////////////////////////

///////////////////////////CONSUMER/////////////////////////////////////

//Compresses the buffer object.

struct output RLECompress(struct buffer temp){
	
	struct output compressed;   
	compressed.count=malloc(temp.last_page_size*sizeof(int));  			// allocat a block of memory for compressed.count
	char* tempString=malloc(temp.last_page_size); 					// allocate a single block of memory with the specefic size for char* tempstring
	int countIndex=0;    								//intialate the countIndex with 0
	
	for(int i=0;i<temp.last_page_size;i++){   
		
		tempString[countIndex]=temp.address[i]; 				// put temp.adress[i] into the array called tempString
		compressed.count[countIndex]=1;    
                
		while(i+1<temp.last_page_size && temp.address[i]==temp.address[i+1]){ 
			
			compressed.count[countIndex]++;
			i++;
		}
		
		countIndex++;
	}
	
	compressed.size=countIndex;
	compressed.data=realloc(tempString,countIndex); 				//data compressed
	return compressed;
}


//Calculates the relative output position for the buffer object.

int calculateOutputPosition(struct buffer temp){
	
	int position=0;
	
	//step 1: Find the relative file position of the buffer object.
	
	for(int i=0;i<temp.file_number;i++){
		position+=pages_per_file[i];
	}
	
	//Step 2: Now we're at the index where the particular file begins, we need to find the page relative position
	
	position+=temp.page_number;
	return position;
}

//Called by consumer threads to start memory compression

void *consumer(){
	
	do{
		pthread_mutex_lock(&lock);  					//lock the critical section
		while(q_size==0 && isComplete==0){
		    pthread_cond_signal(&empty); 				// send a signal to producer that the buffer is empty 
		    pthread_cond_wait(&fill,&lock); 			       //call the producer to start filling the queue.
		}
		if(isComplete==1 && q_size==0){ 				//If producer is done mapping and there's nothing left in the queue.
			pthread_mutex_unlock(&lock);  				 // unlock the critical section 
			return NULL;    					// nothing left in the queue to consume
		}
		
		struct buffer temp=get();   					 // get elements from the buffer and put them in temp 
		
		if(isComplete==0){
		    pthread_cond_signal(&empty);  				// send signal to producer the buffer is empty
		}	
		pthread_mutex_unlock(&lock);
		
		
		int position=calculateOutputPosition(temp);  			// to indixed the position to compress
		out[position]=RLECompress(temp);   		// compress  the position indexed
	}while(!(isComplete==1 && q_size==0));
	return NULL;
}
////////////////////////////////////////////////////////////////////////

///////////////////////////Main/////////////////////////////////////////


void printOutput(){

     char* finalOutput=malloc(total_pages*page_size*(sizeof(int)+sizeof(char)));
     char* init=finalOutput;     //contains the starting point of finalOutput pointer
	
         for(int i=0;i<total_pages;i++){
		if(i<total_pages-1){
			if(out[i].data[out[i].size-1]==out[i+1].data[0]){ //Compare i'th output's last character with the i+1th output's first character
				out[i+1].count[0]+=out[i].count[out[i].size-1];
				out[i].size--;
			}
		}
		
		for(int j=0;j<out[i].size;j++){

			int num=out[i].count[j];
			char character=out[i].data[j];
			*((int*)finalOutput)=num;
			finalOutput+=sizeof(int);
			*((char*)finalOutput)=character;
                        finalOutput+=sizeof(char);
			
		}
	}

	fwrite(init,finalOutput-init,1,stdout);
}


int main(int argc, char* argv[]){

	//Check if less than two arguments
	if(argc<2){
		printf("pzip: file1 [file2 ...]\n");
		exit(1);
	}

	
	
	page_size = 10000000;
	num_files=argc-1; //Number of files, needed for producer.
	total_threads=get_nprocs(); //Number of processes consumer threads 
	pages_per_file=malloc(sizeof(int)*num_files); //Pages per file.
	
	
        out=malloc(sizeof(struct output)* 512000*2); 
	
	pthread_t pid,cid[total_threads];
	pthread_create(&pid, NULL, producer, argv+1);  //argv + 1 to skip argv[0]

	//Create consumer thread to compress all the pages per file.
	for (int i = 0; i < total_threads; i++) {
        pthread_create(&cid[i], NULL, consumer, NULL);
    }

    //Wait for producer-consumers to finish.
    for (int i = 0; i < total_threads; i++) {
        pthread_join(cid[i], NULL);
    }
    pthread_join(pid,NULL);
	printOutput();
	return 0;
}
//////////////////////////////////////////////////////////////////////////
