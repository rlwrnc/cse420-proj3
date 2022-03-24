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

void *open_shared_memory(int req_queue_size)
{
    int fd;
    fd = shm_open("req-queue", O_RDWR, 0666);
    //map memory to *ptr with write permissions
    void *ptr = mmap(0, req_queue_size , PROT_WRITE, MAP_SHARED, fd, 0);
    
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "client: memory map failed\n");
        exit(1);
    }

    return ptr;
}

void read_inputfile(char *inputfile, void *queue_pointer)
{
    FILE *fs;
    const int bufflen = MAXDIRPATH + MAXKEYWORD + 2;
    char buff[bufflen];
    char *token, *context, *exclude, *dirpath, *keyword;

    fs = fopen(inputfile, "r");
    context = NULL, exclude = " \n";

    while (fgets(buff, bufflen, fs) != NULL) {
        dirpath = strtok_r(buff, exclude, &context);
        keyword = strtok_r(NULL, exclude, &context);
        // send request to shared memory, probably in some struct
    }
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

    queue_ptr = open_shared_memory(req_queue_size);

}
