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

typedef struct _node {
	struct _node *next;
	unsigned int size;
	bool inuse;
} Node;

unsigned int amtfree;

Node *head = NULL;
Node *tail = NULL;
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

void setbit(int* number, int pos){
	*number |= 1 << pos;
}

void clearbit(int* number, int pos){
	*number &= ~(1 << pos);
}

int checkbit(int number, int pos){
	return number & (1 << pos);
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

	//initialize our mutex
	pthread_mutex_init(&lock, NULL);

	// open the /dev/zero device
	int fd = open("/dev/zero", O_RDWR);

	// size (in bytes) needs to be evenly divisible by the page size
	head = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (head == MAP_FAILED) { perror("mmap"); exit(1); }

	head->next = NULL;
	head->size = size - sizeof(Node);
	head->inuse = 0;
	amtfree = size;

	// close the device (don't worry, mapping should be unaffected)
	close(fd);

	return 0;
}

int Mem_Available(){
	return amtfree;
}

void* Mem_Alloc(int sizereq){

	//grab the lock
	pthread_mutex_lock(&lock);

	//Pad size to the nearest 8 bytes
	unsigned int newsize = sizereq + sizeof(Node);
	unsigned int sizefix = 8-(newsize%8);
	//printf("Size of user request: %d\n", sizereq);
	//printf("Padding it by: %hu\n", sizefix);
	//printf("With sizeofnode = %lu, total amount given: %hu\n", sizeof(Node), newsize );

	//pad the size of the actual amount (+sizeof node)
	if(sizefix != 8){
		newsize += sizefix;
	}

	if(Mem_Available() < newsize){
		pthread_mutex_unlock(&lock);
		return NULL;
	}
	
	//Pad the user's amount to make sure it works, too
	sizefix = 8-(sizereq % 8);
	if(sizefix != 8){
		sizereq += sizefix;
	}
	

	Node *iterator = head; //Iterator for the list

	//printf("Total Size w/ Node: %hu User Requested Size: %hu\n", newsize, sizereq);
	while(iterator){
		if(iterator->inuse == false && iterator->size >= newsize ){
			break;
		}
		iterator = iterator->next;
	}

	//No matches found
	if(!iterator){ pthread_mutex_unlock(&lock); return NULL; }

	unsigned int backup_size = iterator->size; //previous freespace pointer size

	iterator->size = sizereq;
	iterator->inuse = true;


	//this line is losing a reference to the rest of the list, if we insert earlier....
	
	//If we're at the tail/freespace:
	if(iterator->next == NULL){
		//Make a new node, initialize it
		iterator->next = (void*) ((char*) iterator) + newsize;
	 	// this line is (hopefully now fixed) 200% chance of being bad.
		iterator->next->size = backup_size - newsize;
		iterator->next->inuse = false;
		iterator->next->next = NULL;
	}

	//If we aren't at the freespace
	else{
		//not sure, maybe nothing?
	}


	//Subtract from our amount free counter;
	amtfree -= newsize;

	//printf("Size open yet: %d\n", Mem_Available());

	pthread_mutex_unlock(&lock);
	return (void *)(iterator + sizeof(Node));
}

int Mem_Free(void* ptr){

	pthread_mutex_lock(&lock);


	if(!ptr){ pthread_mutex_unlock(&lock); return -1; }

	Node* header = (Node *) ptr;
	header -= sizeof(Node);

	if(header->inuse == true){
		amtfree += header->size + sizeof(Node);
		if(header->next == NULL){
			//tail node, do something special?
			
		}

	} else {
		pthread_mutex_unlock(&lock);
		return -1;
	}

	header->inuse = false;

	

	// perform a coalescing pass
	Node* current = head;
	while(current){
		if(	   current->inuse == true
			|| current->next == NULL 
			|| current->next->inuse == true){
			current = current->next;
			continue;
		} else {
			// current -> inuse = false AND the next is also not in use
			// it's coalesce time!

			unsigned int backup_size = current->next->size;
			current->next = current->next->next;
			current->size += backup_size + sizeof(Node);
		}
	}

	pthread_mutex_unlock(&lock);

	return 0;
}

void Mem_Dump(){

	pthread_mutex_lock(&lock);

	Node * header = head;
	printf("\n\nDUMPING MEMORY\n\n");
	while(header != NULL){
		int data = 0;
		memcpy(&data, header+sizeof(Node), sizeof(int));
		printf("size: %d, inuse: %d, data(dec): %d, ptr: %p\n", header->size, header->inuse, data, header );
		header = header->next;
	}

	printf("\n\n");
	pthread_mutex_unlock(&lock);
}
