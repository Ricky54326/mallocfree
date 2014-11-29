#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

int amtfree;
int alloc_size = 16;
int header_size = -1;

char *head = NULL;
pthread_mutex_t lock;

// WL1 fails because we aren't able to allocate nearly enough
// 16 byte allocs

// they expect us to use 4032 for 16 byte allocs of data, or
// be able to alloc 16 bytes 252 times. This leaves us
// 64 bytes MAX for ALL HEADER USAGE for the 16 byte alloc WL1

// How do we fix this? Easy! (well, not easy, but I know how)
// We maintain a byte-field!

// 32 bytes is enough to represent 256 separate bits
// we can use the first 32 bytes of the alloc to store a 256 bits
// of "in use or empty" flags. When we need to alloc something,
// we just hand the user back the first empty we find.

void setbit(char* number, int pos){
	//printf("--- %p \n", number);
	*number |= 1 << pos;
}

void clearbit(char* number, int pos){
	*number &= ~(1 << pos);
}

int checkbit(char* number, int pos){
	return *number & (1 << pos);
}

void Header_Set(int pos, int sizereq){

	char *ref = (char*)head + (pos / 2);

	switch(sizereq){
		case 16:
			setbit( ref, (pos % 2) * 4 + 2);
			break;
		case 80:
			setbit( ref, (pos % 2) * 4 + 1);
			break;
		case 256:
			setbit( ref, (pos % 2) * 4 + 1);
			setbit( ref, (pos % 2) * 4 + 2);
			break;
	}

	int i;
	for(i = 0; i < sizereq / 16 ; i++){
		setbit( ref, ((pos+i) % 2) * 4 );
		if( ((pos+i) % 2) == 1){
			ref++;
		}
	}
}

void Header_Clear(int pos){
	printf("Pos: %d\n", pos);
	char *ref = (char*)head + (pos / 2);
	int size_flag_1 = checkbit(ref, (pos % 2) * 4 + 1);
	int size_flag_2 = checkbit(ref, (pos % 2) * 4 + 2);
	int size = 0;
	if(size_flag_1 && size_flag_2){
		size = 256;
	} else if(size_flag_1){
		size = 80;
	} else if(size_flag_2){
		size = 16;
	} else {
		
	}
	amtfree += size;
	int i;
	for(i = 0; i < size / 16; i++){
		clearbit( ref, ((pos+i) % 2) * 4);
		if( ((pos+i) % 2) == 1){
			ref++;
		}
	}
}

int Header_Check(int pos){
	char *ref = (char*)head + (pos / 2);
	return checkbit( ref, (pos % 2) * 4 );
}

int Header_Valid_Start(int pos){
	char *ref = (char*)head + pos /2;
	return checkbit( ref, (pos % 2) * 4 ) 
		&& ( checkbit( ref, (pos % 2) * 4 +1 ) 
				|| checkbit( ref, (pos % 2) * 4 + 2 ) );
}

int Mem_Init(int size){
	
	if(size <= 0 || head){
		return -1;
	} else {
		// may be causing new problems
		if(size % 4096 != 0){ // actually may have fixed them
			size += 4096 - (size % 4096); // acually is dolan
		}
	}

	//printf("Size: %d\n", size);

	// number_of_bits_necessary / 16_byte_allocs / 8bits_in_a_byte * 4bits_bits_per_header_seg
	header_size = size * 4 / 16 / 8; 
	printf("header_size: %d\n", header_size);


	//initialize our mutex
	pthread_mutex_init(&lock, NULL);

	// open the /dev/zero device
	int fd = open("/dev/zero", O_RDWR);

	// size (in bytes) needs to be evenly divisible by the page size
	head = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

	printf("Head: %p\n", head);

	if (head == MAP_FAILED) { perror("mmap"); exit(1); }

	amtfree = size - header_size;

	// close the device (don't worry, mapping should be unaffected)
	close(fd);

	return 0;
}

int Mem_Available(){
	return amtfree;
}

void* Mem_Alloc(int sizereq){

	pthread_mutex_lock(&lock);

	if(sizereq > 256 || sizereq <= 0 || amtfree < sizereq){
		pthread_mutex_unlock(&lock);
		return NULL;
	} else {
		if(sizereq <= 16){
			sizereq = 16;
		} else if(sizereq <= 80){
			sizereq = 80;
		} else if(sizereq <= 256){
			sizereq = 256;
		}
	}

	int pos; // will be pos of open spot, or 256 if failed
	int consecutive_free = 0;
	// header_size_in_bytes * bits_in_byte / bits_for_an_alloc
	for(pos = 0; pos < header_size * 8 / 4; pos++){
		if( ! Header_Check(pos) ){
			consecutive_free++;
			if(consecutive_free * 16 >= sizereq){
				// mark as used
				Header_Set(pos-consecutive_free+1, sizereq);
				amtfree -= sizereq;
				pthread_mutex_unlock(&lock);
				return (void *)head + header_size + (16 * (pos-consecutive_free+1));
			}
		} else {
			consecutive_free = 0;
		}
	}

	pthread_mutex_unlock(&lock);
	return NULL;
}

int Mem_Free(void* ptr){
	pthread_mutex_lock(&lock);

	if(!ptr || ((unsigned long)(ptr-(void*)head) & 15) != 0){ pthread_mutex_unlock(&lock); return -1; }
	int pos = ((void*) ptr - (void*) head - header_size)/16;
	// if( ! Header_Valid_Start(pos) ){
	// 	printf("Exiting:1");
	// 	exit(-1000);
	// }
	Header_Clear(pos);
	pthread_mutex_unlock(&lock);
	return 0;
}

void Mem_Dump(){
	pthread_mutex_lock(&lock);
	char* head_ptr = head;
	int i = 0;
	while(i < 128){
		printf("%x ", (*head_ptr) & 0xff);
		head_ptr++;
		i++;
	}

	pthread_mutex_unlock(&lock);
}
