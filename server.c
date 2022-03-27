/* includes */
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#define MAXDIRPATH 1024
#define MAXKEYWORD 256
#define MAXLINESIZE 1024
#define MAXOUTSIZE 2048

/**
 * @brief contains all data relating to shared memory queue
 *
 */
struct Queue {
    char *buffer;                   //data stored in queue
    unsigned short *overlap;        //points to number of characters left of overlap (if any)
    int front;                      //front of queue (data next served)
    int size;                       //size of queue (not entire shared memory space)
    sem_t *empty;
    sem_t *full;
    sem_t *mutex;
};

/**
 * @brief object defined for storing a filename, linenumber, and line text in a single variable.
 * 
 */
struct Item {
    char *filename;
    int linenumber;
    char *line;
};

/**
 * @brief contains all information about the shared buffer between threads, including semaphores, indicies, and size.
 * 
 */
struct ThreadBufferInfo {
    int use;
    int fill;
    int maxBufferSize;
    sem_t empty;
    sem_t full;
    sem_t mutex;
};

/**
 * @brief passed when a thread is created, has all the necessary information for worker & printer threads.
 * 
 */
struct ThreadArgs {
    char *filename;
    char *keyword;
    struct Item *buffer;
    struct ThreadBufferInfo bufferInfo;
    struct List *list;
};

/**
 * @brief node containing the thread id and attribute of a thread.
 * 
 */
struct Node {
    struct Node *next;
    pthread_attr_t attr;
    pthread_t tid;
};

/**
 * @brief linked-list to keep track of number of threads opened and store thread information.
 * 
 */
struct List {
    struct Node *head;
    struct Node *tail;
    int threadcount;
};

/**
 * @brief Creates the node
 * 
 * @return struct Node* 
 */
struct Node *create_node(void) {
    struct Node *node = malloc(sizeof(struct Node));
    if (node == NULL) {
        fprintf (stderr, "%s: Couldn't create memory for the node; %s\n", "linkedlist", strerror(errno));
        exit(-1);
    }
    node->tid = 0;
    node->next = NULL;
    return node;
}

/**
 * @brief Creates the linked-list
 * 
 * @return struct List* 
 */
struct List *create_list(void) {
    struct List *list = malloc(sizeof(struct List));
    if (list == NULL) {
       fprintf (stderr, "%s: Couldn't create memory for the list; %s\n", "linkedlist", strerror (errno));
       exit(-1);
    }
    list->head = NULL;
    list->tail = NULL;
    list->threadcount = 0;
    return list;
}

/**
 * @brief adds new node to the end of a list.
 * 
 * @param node 
 * @param list 
 */
void insert_tail(struct Node *node, struct List *list) {
    if(list->head == NULL && list->tail == NULL) {
        list->head = node;
        list->tail = node;
        list->threadcount++;
    } else {
        list->tail->next = node;
        list->tail = node;
        list->threadcount++;
    }
}

/**
 * @brief destroys list and frees all memory allocation.
 * 
 * @param list 
 */
void destroy_list(struct List *list) {
  struct Node *ptr = list->head;
  struct Node *tmp;  
  while (ptr != NULL) {
    tmp = ptr;
    ptr = ptr->next;
    free(tmp);
  }
  free(list);
}

/**
 * @brief Create a thread args object to pass when a thread is created
 * 
 * @param filename 
 * @param keyword 
 * @param buffer 
 * @param bufferInfo 
 * @param list 
 * @return struct ThreadArgs* 
 */
struct ThreadArgs *create_thread_args(char *filename, char *keyword, struct Item *buffer, struct ThreadBufferInfo bufferInfo, struct List *list)
{
    struct ThreadArgs *threadargs = malloc(sizeof(struct ThreadArgs));
    if (threadargs == NULL) {
        fprintf (stderr, "%s: Couldn't create memory for the thread arguments; %s\n", "linkedlist", strerror(errno));
        exit(-1);
    }
    threadargs->filename = strdup(filename);
    threadargs->keyword = strdup(keyword);
    threadargs->buffer = buffer;
    threadargs->bufferInfo = bufferInfo;
    threadargs->list = list;
    return threadargs;
}
/**
 * @brief adds an Item to the shared buffer
 * 
 * @param item - the item to be added to the buffer
 * @param bufferInfo - information about the buffer including the array indices.
 * @param buffer 
 */
void buffer_fill(struct Item item, struct ThreadBufferInfo bufferInfo, struct Item *buffer)
{
    buffer[bufferInfo.fill] = item;
    bufferInfo.fill++;
    if (bufferInfo.fill == bufferInfo.maxBufferSize) {
        bufferInfo.fill = 0;
    }
}
/**
 * @brief helper function that returns an Item retrieved from the shared buffer
 * 
 * @param bufferInfo contains the buffer indices use and fill
 * @param buffer the shared buffer between threads
 * @return struct Item*
 */
struct Item *buffer_get(struct ThreadBufferInfo bufferInfo, struct Item *buffer)
{
    struct Item *tmp = &buffer[bufferInfo.use];
    bufferInfo.use++;
    if(bufferInfo.use == bufferInfo.maxBufferSize) {
        bufferInfo.use = 0;
    }
    return tmp;
}
/**
 * @brief Used by worker threads to find keywords in a file and add lines to the shared thread buffer.
 * 
 * @param ThreadArgs - contains requested keyword, filename, semaphores, buffer indices, and the buffer itself
 * @return void* - doesn't return anything, exits after adding dummy value to the shared buffer.
 */
void *retrieve_keyword(void* ThreadArgs)
{
   FILE *fileptr = fopen(((struct ThreadArgs *)ThreadArgs)->filename, "r");
   char filebuffer[MAXLINESIZE];
   char delim[] = " \t\n";
   char *token;
   char *saveptr = filebuffer;
   int currentline = 0; //may need to change if line numbers do not start at 0

   while(fgets(filebuffer, 1024, fileptr)) {
       token = strtok_r(filebuffer, delim, &saveptr);
       while(token != NULL) {
           if(strcmp(token, ((struct ThreadArgs *)ThreadArgs)->keyword) == 0) {
               //create item and store the line information
               struct Item item;
               item.filename = ((struct ThreadArgs *)ThreadArgs)->filename;
               item.line = filebuffer;
               item.linenumber = currentline;
               //add to buffer -- CRITICAL SECTION
               sem_wait(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.empty);
               sem_wait(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.mutex);
               buffer_fill(item, ((struct ThreadArgs *)ThreadArgs)->bufferInfo, ((struct ThreadArgs *)ThreadArgs)->buffer);
               sem_post(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.mutex);
               sem_post(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.full);
               //buffer stuff above -- EXIT CRITICAL SECTION
               //increment currentline and move to next line
               currentline++;
               continue; //breaks out of inner while loop and gets next line to be parsed.
           }
           else {
               strtok_r(NULL, delim, &saveptr);
                //keep searching tokens
           }
       }
   }
   //create the dummyItem
   struct Item dummyItem;
   dummyItem.filename = "";
   dummyItem.line = "";
   dummyItem.linenumber = -1;
   //input the dummy item into the buffer, with care for the critical section.
   sem_wait(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.empty);
   sem_wait(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.mutex);
   buffer_fill(dummyItem, ((struct ThreadArgs *)ThreadArgs)->bufferInfo, ((struct ThreadArgs *)ThreadArgs)->buffer);
   sem_post(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.mutex);
   sem_post(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.full);
   free(((struct ThreadArgs *)ThreadArgs)->filename);
   free(((struct ThreadArgs *)ThreadArgs)->keyword);
   free((struct ThreadArgs *)ThreadArgs);
   fclose(fileptr);
   pthread_exit(0);
}

/**
 * @brief Used by printer thread, pulls an item out of the buffer and outputs it to file, 
 *        this will also wait for all worker threads to close
 * 
 * @param ThreadArgs - contains all necessary information for the printer thread. 
 * @return void* 
 */
void *print_buffer(void* ThreadArgs) //aka CONSUMER
{
    char *outputfilename = "output.txt";
    struct flock fl = {F_WRLCK, SEEK_END, 0, MAXOUTSIZE, 0}; //might need to alter the 5th arg for multiple processes.
    int filestream = open(outputfilename, O_WRONLY);
    struct Item *ptr = malloc(sizeof(struct Item));
    ptr->linenumber = 0;
    while(((struct ThreadArgs *)ThreadArgs)->list->threadcount != 0) {
        //protects shared buffer between threads.
        sem_wait(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.full);
        sem_wait(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.mutex);
        ptr = buffer_get(((struct ThreadArgs*)ThreadArgs)->bufferInfo, ((struct ThreadArgs *)ThreadArgs)->buffer);
        sem_post(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.mutex);
        sem_post(&((struct ThreadArgs *)ThreadArgs)->bufferInfo.empty);
        if(ptr->linenumber != -1) {
            //create & format the line to be outputted
            char linebuff[MAXOUTSIZE] = "";
            char linenumber[5];
            strcat(linebuff, ptr->filename);
            strcat(linebuff, ":");
            sprintf(linenumber, "%d", ptr->linenumber);
            strcat(linebuff, linenumber);
            strcat(linebuff, ":");
            strcat(linebuff, ptr->line);
            //try to lock the file
            fl.l_type = F_WRLCK;
            fcntl(filestream, F_SETLKW, &fl);
            //if file is currently locked, thread will wait.
            write(filestream, linebuff, MAXOUTSIZE);
            //release the lock
            fl.l_type = F_UNLCK;
            fcntl(filestream, F_SETLK, &fl);
        }
        else {
            ((struct ThreadArgs *)ThreadArgs)->list->threadcount--;
        }
    }
    free(ptr);
    free(((struct ThreadArgs *)ThreadArgs)->filename);
    free(((struct ThreadArgs *)ThreadArgs)->keyword);
    free((struct ThreadArgs *)ThreadArgs);
    close(filestream);
    pthread_exit(0);
}

/**
 * @brief searches through the base directory for a process, creating threads each time a file is found.
 * 
 * @param directory - directory path passed when process is handled
 * @param dir - directory stream to read from
 * @param dirent - struct used for directory operations
 * @param keyword - the requested keyword from the process
 * @param buffer - the shared buffer between threads
 * @param bufferInfo - information about the shared buffer
 * @param list - the linked-list of thread id's
 */
void search_directory(char* directory, DIR *dir, struct dirent *dirent, char* keyword, struct Item *buffer, struct ThreadBufferInfo bufferInfo, struct List *list)
{
    while((dirent = readdir(dir)) != NULL) {
        //create a string with directory path to input to stat()
        char dirpath[strlen(directory) + strlen(dirent->d_name)];
        strcpy(dirpath, directory);
        strcat(dirpath, "/");
        strcat(dirpath, dirent->d_name);
        //get stat output for directory
        struct stat statbuf;
        stat(dirpath, &statbuf);

        if(dirent->d_name[0] != '.') {
            if(S_ISREG(statbuf.st_mode)) {
                //creating file-search threads
                struct Node *newNode = create_node();
                struct ThreadArgs *threadargs = create_thread_args(directory, keyword, buffer, bufferInfo, list);
                pthread_attr_init(&newNode->attr);
                pthread_attr_setdetachstate(&newNode->attr, PTHREAD_CREATE_JOINABLE);
                pthread_create(&newNode->tid, &newNode->attr, retrieve_keyword, (void *) threadargs);
                insert_tail(newNode, list);
            }
        }
    }
    //creating printer thread
    struct Node *printerNode = create_node();
    struct ThreadArgs *threadargs = create_thread_args(directory, keyword, buffer, bufferInfo, list);
    pthread_attr_init(&printerNode->attr);
    pthread_attr_setdetachstate(&printerNode->attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&printerNode->tid, &printerNode->attr, print_buffer, (void *) threadargs);
    closedir(dir);
}

/**
 * @brief function used right after process creation to begin handling the client request
 * 
 * @param request - request from client; contains directory_path and keyword delimited by space
 * @param buffer_size - buffer size defined by the client.
 */
void handle_client_request(char *request, int buffer_size) {
   char *directory_path, *keyword, *context = NULL, *exclude = " ";

   directory_path = strtok_r(request, exclude, &context);
   keyword = strtok_r(NULL, exclude, &context);

   struct List *list = create_list();
   struct Item *buffer;
   struct ThreadBufferInfo *bufferInfo;
   buffer = malloc(sizeof(struct Item) * buffer_size);
   bufferInfo = malloc(sizeof(struct ThreadBufferInfo));
   sem_init(&bufferInfo->empty, 0, buffer_size);
   sem_init(&bufferInfo->full, 0, 0);
   sem_init(&bufferInfo->mutex, 0, 1);

   DIR *dir = NULL;
   struct dirent *dirent = NULL;
   dir = opendir(directory_path);
   search_directory(directory_path, dir, dirent, keyword, buffer, *bufferInfo, list);

   free(buffer);
   free(bufferInfo);
}

/**
 * @brief creates shared memory region for queue
 *
 * @param size - size of queue (in bytes)
 */ 
void *create_shared_memory(int size)
{
    int fd;
    void *ptr;

    fd = shm_open("queue", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, size + 1 + 2);                                                    //+1 for null character, +2 for overlap flag

    ptr = mmap(0, size + 3, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "server: memory map failed\n");
        exit(1);
    }

    return ptr;
}

/**
 * @brief creates a queue on shared memory
 *
 * @param size - size of queue (in # requests)
 */
struct Queue create_queue(int size)
{
    int total_size, line_size;
    struct Queue q;
    
    line_size = MAXDIRPATH + 1 + MAXKEYWORD + 1;                                    //+1 for space, +1 for null character
    total_size = size * line_size;
    q.buffer = (char *) create_shared_memory(total_size);
    q.overlap = (unsigned short *) (q.buffer + total_size + 1);                     //+1 to account for null character
    *q.overlap = 0;
    q.front = 0;
    q.size = total_size;
    q.empty = sem_open("/empty", O_CREAT, 0666, size);
    q.full = sem_open("/full", O_CREAT, 0666, 0);
    q.mutex = sem_open("/mutex", O_CREAT, 0666, 1);
    return q;
}

/**
 * @brief unlinks all shared memory used for queue
 *
 * @param queue to be unlinked
 */
void unlink_queue(struct Queue *q)
{
    shm_unlink("queue");
    sem_close(q->empty);
    sem_unlink("/empty");
    sem_close(q->full);
    sem_unlink("/full");
    sem_close(q->mutex);
    sem_unlink("/mutex");
}

/**
 * @brief dequeues request string on shared memory queue
 *
 * @param request_buffer - holds dequeued request string
 * @param q - reference to queue
 */
void dequeue(char *request_buffer, struct Queue *q)
{
    unsigned short reqlen;

    sem_wait(q->full);
    sem_wait(q->mutex);

    if (*q->overlap != 0 || q->front != q->size - *q->overlap) 
        strcpy(request_buffer, &q->buffer[q->front]);
    else {
        strncpy(request_buffer, &q->buffer[q->front], *q->overlap);
        strcpy(&request_buffer[*q->overlap], q->buffer);
        *q->overlap = 0;
    }
    
    sem_post(q->mutex);
    sem_post(q->empty);

    reqlen = strlen(request_buffer) + 1;
    q->front = (q->front + reqlen) % q->size;
}

/**
 * @brief forks search process for each incoming queue request
 *
 * @param req_queue_size - size of request queue
 * @param buffersize - size of shared thread buffer
 */
void watch_queue(int req_queue_size, int buffersize)
{
    int process_count;
    char request_buffer[MAXDIRPATH + MAXKEYWORD + 2];
    struct Queue q;
    pid_t pid;
    
    request_buffer[0] = '\0';
    process_count = 0;
    q = create_queue(req_queue_size);
    pid = 1;
    
    while (pid != 0 && strcmp(request_buffer, "exit") != 0) {
        dequeue(request_buffer, &q);
        process_count++;
        pid = fork();
    }
    
    if (pid < 0) {
        fprintf(stderr, "server: process fork failed\n");
        exit(1);
    } else if (pid == 0) {
        handle_client_request(request_buffer, buffersize);
    } else if (pid != 0) {
        for (int i = 0; i < process_count; i++)
            wait(NULL);
        unlink_queue(&q);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "server usage: ./server <req-queue-size> <buffersize>\n");
        return 1;
    }

    int req_queue_size = atoi(argv[1]);
    int buffersize = atoi(argv[2]);
    watch_queue(req_queue_size, buffersize);
    return 0;
}
