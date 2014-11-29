#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <string.h>

typedef struct _node {
	struct _node *next;
	struct _node *prev;
	unsigned int size;
	bool inuse;
} Node;

unsigned int amtfree;
unsigned int freecount;
int totalsize = 0;

Node *head = NULL;
Node *last_used = NULL;


int Mem_Init(int size){
	//printf("size: %lu\n", sizeof(Node));
	if(size <= 0 || head){
		return -1;
	} else {
		// may be causing new problems
		if(size % 4096 != 0){ // actually may have fixed them
			size += 4096 - (size % 4096); // acually is dolan
		}
		
	}
	totalsize = size;

	// open the /dev/zero device
	int fd = open("/dev/zero", O_RDWR);

	// size (in bytes) needs to be evenly divisible by the page size
	head = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (head == MAP_FAILED) { perror("mmap"); exit(1); }
	last_used = head;

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
	printf("Requested: %d\n", sizereq);
	//Pad size to the nearest 8 bytes
	unsigned int sizefix = 8-(sizereq % 8);
	if(sizefix != 8){
		sizereq += sizefix;
	}
	unsigned int newsize = sizereq + sizeof(Node);

	// sizefix = 8 - ( newsize % 8 );

	// //pad the size of the actual amount (+sizeof node)
	// if(sizefix != 8){
	// 	newsize += sizefix;
	// }

	if(Mem_Available() < newsize){
		printf("Uh oh");
		return NULL;
	}

	printf("Made it");
	
	//Pad the user's amount to make sure it works, too
	sizefix = 8-(sizereq % 8);
	if(sizefix != 8){
		sizereq += sizefix;
	}
	

	Node *iterator = last_used; //Iterator for the list

	//printf("Total Size w/ Node: %hu User Requested Size: %hu\n", newsize, sizereq);
	while(iterator){
		if(iterator->inuse == false && iterator->size >= newsize ){
			last_used = iterator;
			break;
		}
		iterator = iterator->next;
	}

	if(! iterator ){
		// try another pass, starting at the beginning, instead of last_used
		iterator = head;
		while(iterator){
			if(iterator->inuse == false && iterator->size >= newsize ){
				last_used = iterator;
				break;
			}
			iterator = iterator->next;
		}
	}

	//No matches found
	if(!iterator){ 
		printf("failed here");
		return NULL; 
	}

	unsigned int backup_size = iterator->size; //previous freespace pointer size

	iterator->size = sizereq;
	iterator->inuse = true;


	//this line is losing a reference to the rest of the list, if we insert earlier....
	
	//If we're at the tail/freespace:
	if(iterator->next == NULL && amtfree > sizeof(Node)){ 
		//Make a new node, initialize it
		iterator->next = (void*) ((char*) iterator) + newsize;
	 	// this line is (hopefully now fixed) 200% chance of being bad.
		iterator->next->size = backup_size - sizereq;
		iterator->next->inuse = false;
		iterator->next->prev = iterator;
		iterator->next->next = NULL;
	}

	//If we aren't at the freespace
	else{
		//not sure, maybe nothing?
	}


	//Subtract from our amount free counter;
	amtfree -= newsize;

	//printf("Size open yet: %d\n", Mem_Available());

	return (void *)(iterator + sizeof(Node));
}

int Mem_Free(void* ptr){
	if(!ptr){ return -1; }

	Node* header = (Node *) ptr;
	header -= sizeof(Node);

	if(header->inuse == true){
		amtfree += header->size + sizeof(Node);
	} else {
		return -1;
	}

	bool last_used_reset = (last_used == ptr);

	header->inuse = false;

	if(header->prev != NULL && header->prev->inuse == false){
		header = header->prev;
		header->size = header->size + header->next->size;
		header->next = header->next->next;
		if(header->next != NULL){
			header->next->prev = header;
		}
		if(last_used_reset){
			last_used = header;
		}
	}

	if(header->next != NULL && header->next->inuse == false){
		header->size = header->size + header->next->size;
		header->next = header->next->next;
		if(header->next != NULL){
			header->next->prev = header;
		}
	}

	if(last_used_reset){
		last_used = NULL;
	}
	

	return 0;
}

void Mem_Dump(){
	Node * header = head;
	printf("\n\nDUMPING MEMORY\n\n");
	while(header != NULL){
		int data = 0;
		memcpy(&data, header+sizeof(Node), sizeof(int));
		printf("size: %d, inuse: %d, data(dec): %d, ptr: %p, next: %p, prev: %p\n", header->size, header->inuse, data, header, header->next, header->prev );
		header = header->next;
	}

	printf("\n\n");
}
