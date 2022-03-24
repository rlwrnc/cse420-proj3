#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

/* declarations */
void *open_shared_memory(int req_queue_size);

/* functions */

/*
 *      creates shared memory region for request queue
 *      INPUT: size of request queue
 *      OUTPUT: pointer to shared memory
 */
void *create_shared_memory(int req_queue_size)
{
    int fd;
    fd = shm_open("req-queue", O_CREAT | O_RDONLY, 0666);
    ftruncate(fd, req_queue_size);
    //map memory to *ptr with read permissions
    void *ptr = mmap(0, req_queue_size, PROT_READ, MAP_SHARED, fd, 0);
    
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "client: memory map failed\n");
        exit(1);
    }

    return ptr;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "server usage: ./server <req-queue-size> <buffersize>\n");
        return 1;
    }

    int req_queue_size = atoi(argv[1]);
    void *queue_ptr;

    queue_ptr = create_shared_memory(req_queue_size);
}
