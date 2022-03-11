/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

/* declarations */
#define MAXDIRPATH 1024
#define MAXKEYWORD 256


void *create_shared_memory(int req_queue_size);
void read_inputfile(char *inputfile, void *queue_pointer);

/* functions */

void *create_shared_memory(int req_queue_size)
{
    int fd;
    fd = shm_open("req-queue", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, req_queue_size);
    void *ptr = mmap(0, req_queue_size , PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "client: memory map failed\n");
        exit(1);
    }

    return ptr;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "client usage: ./client <req-queue-size> <inputfile>\n");
        return 1;
    }
    
    char *inputfile = argv[2];
    int req_queue_size = atoi(argv[1]);
    void *queue_ptr;

    queue_ptr = create_shared_memory(req_queue_size);
    
}
