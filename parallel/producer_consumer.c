/*
 * A program that takes a series of files and tokenizes them.
 * Uses OpenMP to create a producer-consumer method of tokenizing
 * where each file has one producer that puts each line in a shared queue
 * and a pool of consumers that read each line and tokenize it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

typedef struct q_node {
	struct q_node* next;
	void* data;
} node;

typedef struct queue_type {
	node head;
	node tail;
	int enqueued;
	int dequeued;
} queue;

void init_queue(queue** q);

void destroy_queue(queue** q);

int enqueue(queue* q, void* line);

void* dequeue(queue* q);

void dequeue_helper(queue* q, void** ret_value);

int size(queue* q);

void produce(char* file, int max_queue_size);

void consume();

int done();

void print_queue(queue* q);

int thread_count, finished, num_files;
queue* lines;

int main(int argc, char* argv[]) {
	int max_queue_size;
	/* check if at least 1 file specified */
	if (argc < 3) {
		printf("Usage: %s <max_queue_size> <file1> <file2> ... <fileN>\n", argv[0]);
		exit(0);
	}
	max_queue_size = strtol(argv[1], NULL, 10);
	num_files = argc - 2;
	finished = 0;
	
	/* has roughly half as many consumers as producers */
	thread_count = num_files + (num_files/2) + 1;
	init_queue(&lines);
	
#	pragma omp parallel num_threads(thread_count)
	{
		int rank = omp_get_thread_num();
		/* first num_files ranks are producers */
		if (rank < num_files)
			produce(argv[2 + rank], max_queue_size);
		else
			consume();
	}
	
	destroy_queue(&lines);
	return 0;
}

/*
 * initializes a queue
 */
void init_queue(queue** q) {
	*q = malloc(sizeof(queue));
	((*q)->head).next = &((*q)->tail);
	((*q)->head).data = NULL;
	((*q)->tail).next = &((*q)->head);
	((*q)->tail).data = NULL;
	(*q)->enqueued = 0;
	(*q)->dequeued = 0;
}

/*
 * destroys the queue, making sure to deallocate all nodes
 */
void destroy_queue(queue** q) {
	while (size(*q) > 0)
		free(dequeue(*q));
	free(*q);
}

/*
 * Adds a new node containing the string line at the end of queue q
 * returns -1 in case of some error
 */
int enqueue(queue* q, void* data) {
	node* new_node = malloc(sizeof(node));
	node* last_node = (q->tail).next;
	last_node->next = new_node;
	(q->tail).next = new_node;
	new_node->data = data;
	new_node->next = &(q->tail);
	(q->enqueued)++;
	return 1;
}

/*
 * Returns the item at the front of the queue
 * this method takes care of the critical sections
 */
void* dequeue(queue* q) {
	void* ret_val = NULL;
	int q_size = size(q);
	if (q_size < 2) 
#	pragma omp critical(enqueue)
	{
		dequeue_helper(q, &ret_val);
		if (size(q) == 0) 
			(q->tail).next = &(q->head);
	}
	else
		dequeue_helper(q, &ret_val);
		
	return ret_val;
}

/*
 * helper method for the dequeue method, does the actual dequeuing 
 */
void dequeue_helper(queue* q, void** ret_value) {
	/* empty queue */
	if ((q->head).next == &(q->tail) || size(q) == 0) {
		*ret_value = NULL;
		return;
	}
	
	node* n = (q->head).next;
	(q->head).next = n->next;
	*ret_value = n->data;
	free(n);
	(q->dequeued)++;
}

/*
 * Returns size of the queue
 */
int size(queue* q) {
	return (q->enqueued) - (q->dequeued);
}

/*
 * runs the producer thread, adding lines to the queue and reading them
 * from file
 */
 void produce(char* file, int max_queue_size) {
	FILE* fp;
	char* line = malloc(255*sizeof(char));
	
	fp = fopen(file, "r");
	if (fp == NULL) {
		printf("Could not open file: \"%s\"\n", file);
		exit(0);
	}
	
	/* goes through file line by line */
	while (fgets(line, 255, fp) != NULL) {
		/* wait until queue has room */
#		pragma omp critical(enqueue)
		{
			while (!(size(lines) <= max_queue_size));
			enqueue(lines, line);
		}
		line = malloc(255*sizeof(char));
	}
	
	free(line);
	fclose(fp);
	/* update finished counter */
#	pragma omp atomic
	finished++;
	
	/* finished producing, may as well try to consume */
	consume();
 }
 
 /*
  * runs the consumer thread, getting lines from the queue 
  */
 void consume() {
	char *saveptr, *token, *line, *str_ptr;
	char* del = " \n";
	int rank = omp_get_thread_num();
	//if (rank<1000) return;
	while (!done()) {
		line = NULL;
#		pragma omp critical(dequeue)
		{
			while (size(lines) == 0 && !done());
			line = dequeue(lines);
		}
		
		if (line != NULL) {
			str_ptr = line;
			/* tokenize line */
			token = strtok_r(line, del, &saveptr);
			while (token != NULL) {
				printf("%d: %s\n", rank, token);
				token = strtok_r(NULL, del, &saveptr);
			}
			free(str_ptr);
		}
	}
 }
 
 /*
  * determines if all producers are done
  *	finished is incremented once for each file
  */
 int done() {
	return (finished == num_files) && (size(lines) == 0);
 }
 
 void print_queue(queue* q) {
	printf("(%p)HEAD\n->", &(q->head));
	node* n = (q->head).next;
	while (n != &(q->tail)) {
		printf("(%p)%s -> ", n, (char *) (n->data));
		n = n->next;
	}
	printf("(%p)TAIL\n", &(q->tail));
	printf("last node: (%p)%s\n", (q->tail).next, (char *)(((q->tail).next)->data));
 }
