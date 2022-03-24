/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

/* declarations */
void *create_shared_memory(int req_queue_size);
void process_requests(void *front_of_queue, int req_queue_size);

/* functions */

/*
 *      creates shared memory region for request queue
 *      INPUT: size of request queue
 *      OUTPUT: pointer to shared memory
 */
void *create_shared_memory(int req_queue_size)
{
    int fd;
    fd = shm_open("req-queue", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, req_queue_size + 1);                                      //+1 for count variable
    //map memory to *ptr with read permissions
    void *ptr = mmap(0, req_queue_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED) {
        fprintf(stderr, "client: memory map failed\n");
        exit(1);
    }
    
    sprintf(ptr + req_queue_size + 1, "%d", 0);                             //initialize 'count'

    return ptr;
}

void process_requests(void *front_of_queue, int req_queue_size)
{
    int *count;
    char *request;
    void *beginning;
    
    count = front_of_queue + req_queue_size + 1;
    beginning = front_of_queue;

    while (*count == 0)
        ;
    request = front_of_queue;
    //do stuff
    if (front_of_queue++ == count)                                          //no modulo workaround
        front_of_queue -= req_queue_size;
    *count--;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "server usage: ./server <req-queue-size> <buffersize>\n");
        return 1;
    }

    int req_queue_size = atoi(argv[1]);
    void *front_of_queue;

    front_of_queue = create_shared_memory(req_queue_size);
}
