/* File:      histogram.c
 * A program that generates a bunch of random data and stores them in different bins.
 * It then displays the data as a histogram
 * The methods that I wrote are main, thread_func, and barrier.
 * The rest was provided by the instructor.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int* bin_counts;
pthread_mutex_t* locks; //locks for each bin
float min_meas, max_meas;
float* bin_maxes;
int data_count, bin_count, thread_count;
float* data;
pthread_t* thread_handles;
/* barrier variables */
int barrier_counter = 0;
pthread_mutex_t barrier_lock;
pthread_cond_t barrier_cond;
   
void Usage(char prog_name[]);

void Get_args(
      char*    argv[]        /* in  */,
      int*     bin_count_p   /* out */,
      float*   min_meas_p    /* out */,
      float*   max_meas_p    /* out */,
      int*     data_count_p  /* out */,
	  int*     thread_count_p);

void Gen_data(
      float   min_meas    /* in  */, 
      float   max_meas    /* in  */, 
      float   data[]      /* out */,
      int     data_count  /* in  */);

void Gen_bins(
      float min_meas      /* in  */, 
      float max_meas      /* in  */, 
      float bin_maxes[]   /* out */, 
      int   bin_counts[]  /* out */, 
      int   bin_count     /* in  */);

int Which_bin(
      float    data         /* in */, 
      float    bin_maxes[]  /* in */, 
      int      bin_count    /* in */, 
      float    min_meas     /* in */);

void Print_histo(
      float    bin_maxes[]   /* in */, 
      int      bin_counts[]  /* in */, 
      int      bin_count     /* in */, 
      float    min_meas      /* in */);
	  
void *Thread_func(void *rank);

void Barrier();

int main(int argc, char* argv[]) {
   long i;
   
   /* Check and get command line args */
   if (argc != 6) Usage(argv[0]); 
   Get_args(argv, &bin_count, &min_meas, &max_meas, &data_count, &thread_count);

   /* Allocate arrays needed */
   bin_maxes = malloc(bin_count*sizeof(float));
   bin_counts = malloc(bin_count*sizeof(int));
   data = malloc(data_count*sizeof(float));
   locks = malloc(bin_count*sizeof(pthread_mutex_t));
   thread_handles = malloc (thread_count*sizeof(pthread_t));
   
   printf("Generating Data...\n");
   /* Generate the data, not generated in parallel */
   Gen_data(min_meas, max_meas, data, data_count);
   printf("Data Generated!\n");
   
   /* Create bins for storing counts */
   Gen_bins(min_meas, max_meas, bin_maxes, bin_counts, bin_count);

   /* initialize locks */
   for (i=0; i<bin_count; i++) {
      pthread_mutex_init(&locks[i], NULL);
   }
   pthread_mutex_init(&barrier_lock, NULL);
   pthread_cond_init(&barrier_cond, NULL);
   
   /* create threads */
   for (i = 0; i < thread_count; i++)
      pthread_create(&thread_handles[i], NULL, Thread_func, (void*) i);

	  
	  
	/* join threads */
   for (i = 0; i < thread_count; i++)
      pthread_join(thread_handles[i], NULL);
   
   /* destroy locks */
   for (i=0; i<bin_count; i++) {
      pthread_mutex_destroy(&locks[i]);
   }
   pthread_mutex_destroy(&barrier_lock);
   pthread_cond_destroy(&barrier_cond);

#  ifdef DEBUG
   printf("bin_counts = ");
   for (i = 0; i < bin_count; i++)
      printf("%d ", bin_counts[i]);
   printf("\n");
#  endif


   free(data);
   free(bin_maxes);
   free(bin_counts);
   free(locks);
   free(thread_handles);
   return 0;

}  /* main */


/*---------------------------------------------------------------------
 * Function:  Usage 
 * Purpose:   Print a message showing how to run program and quit
 * In arg:    prog_name:  the name of the program from the command line
 */
void Usage(char prog_name[] /* in */) {
   fprintf(stderr, "usage: %s ", prog_name); 
   fprintf(stderr, "<thread_count> <bin_count> <min_meas> <max_meas> <data_count>\n");
   exit(0);
}  /* Usage */


/*---------------------------------------------------------------------
 * Function:  Get_args
 * Purpose:   Get the command line arguments
 * In arg:    argv:  strings from command line
 * Out args:  bin_count_p:   number of bins
 *            min_meas_p:    minimum measurement
 *            max_meas_p:    maximum measurement
 *            data_count_p:  number of measurements
 */
void Get_args(
      char*    argv[]        /* in  */,
      int*     bin_count_p   /* out */,
      float*   min_meas_p    /* out */,
      float*   max_meas_p    /* out */,
      int*     data_count_p  /* out */,
	  int*     thread_count_p) {

   *bin_count_p = strtol(argv[2], NULL, 10);
   *min_meas_p = strtof(argv[3], NULL);
   *max_meas_p = strtof(argv[4], NULL);
   *data_count_p = strtol(argv[5], NULL, 10);
   *thread_count_p = strtol(argv[1], NULL, 10);

#  ifdef DEBUG
   printf("bin_count = %d\n", *bin_count_p);
   printf("min_meas = %f, max_meas = %f\n", *min_meas_p, *max_meas_p);
   printf("data_count = %d\n", *data_count_p);
#  endif
}  /* Get_args */


/*---------------------------------------------------------------------
 * Function:  Gen_data
 * Purpose:   Generate random floats in the range min_meas <= x < max_meas
 * In args:   min_meas:    the minimum possible value for the data
 *            max_meas:    the maximum possible value for the data
 *            data_count:  the number of measurements
 * Out arg:   data:        the actual measurements
 */
void Gen_data(
        float   min_meas    /* in  */, 
        float   max_meas    /* in  */, 
        float   data[]      /* out */,
        int     data_count  /* in  */) {
   int i;

   srandom(0);
   for (i = 0; i < data_count; i++)
      data[i] = min_meas + ((double)(max_meas - min_meas))*random()/((double) RAND_MAX);

#  ifdef DEBUG
   printf("data = ");
   for (i = 0; i < data_count; i++)
      printf("%4.3f ", data[i]);
   printf("\n");
#  endif
}  /* Gen_data */


/*---------------------------------------------------------------------
 * Function:  Gen_bins
 * Purpose:   Compute max value for each bin, and store 0 as the
 *            number of values in each bin
 * In args:   min_meas:   the minimum possible measurement
 *            max_meas:   the maximum possible measurement
 *            bin_count:  the number of bins
 * Out args:  bin_maxes:  the maximum possible value for each bin
 *            bin_counts: the number of data values in each bin
 */
void Gen_bins(
      float min_meas      /* in  */, 
      float max_meas      /* in  */, 
      float bin_maxes[]   /* out */, 
      int   bin_counts[]  /* out */, 
      int   bin_count     /* in  */) {
   float bin_width;
   int   i;

   bin_width = (max_meas - min_meas)/bin_count;

   for (i = 0; i < bin_count; i++) {
      bin_maxes[i] = min_meas + (i+1)*bin_width;
      bin_counts[i] = 0;
   }

#  ifdef DEBUG
   printf("bin_maxes = ");
   for (i = 0; i < bin_count; i++)
      printf("%4.3f ", bin_maxes[i]);
   printf("\n");
#  endif
}  /* Gen_bins */


/*---------------------------------------------------------------------
 * Function:  Which_bin
 * Purpose:   Use binary search to determine which bin a measurement 
 *            belongs to
 * In args:   data:       the current measurement
 *            bin_maxes:  list of max bin values
 *            bin_count:  number of bins
 *            min_meas:   the minimum possible measurement
 * Return:    the number of the bin to which data belongs
 * Notes:      
 * 1.  The bin to which data belongs satisfies
 *
 *            bin_maxes[i-1] <= data < bin_maxes[i] 
 *
 *     where, bin_maxes[-1] = min_meas
 * 2.  If the search fails, the function prints a message and exits
 */
int Which_bin(
      float   data          /* in */, 
      float   bin_maxes[]   /* in */, 
      int     bin_count     /* in */, 
      float   min_meas      /* in */) {
   int bottom = 0, top =  bin_count-1;
   int mid;
   float bin_max, bin_min;

   while (bottom <= top) {
      mid = (bottom + top)/2;
      bin_max = bin_maxes[mid];
      bin_min = (mid == 0) ? min_meas: bin_maxes[mid-1];
      if (data >= bin_max) 
         bottom = mid+1;
      else if (data < bin_min)
         top = mid-1;
      else
         return mid;
   }

   /* Whoops! */
   fprintf(stderr, "Data = %f doesn't belong to a bin!\n", data);
   fprintf(stderr, "Quitting\n");
   exit(-1);
}  /* Which_bin */


/*---------------------------------------------------------------------
 * Function:  Print_histo
 * Purpose:   Print a histogram.  The number of elements in each
 *            bin is shown by an array of X's.
 * In args:   bin_maxes:   the max value for each bin
 *            bin_counts:  the number of elements in each bin
 *            bin_count:   the number of bins
 *            min_meas:    the minimum possible measurment
 */
void Print_histo(
        float  bin_maxes[]   /* in */, 
        int    bin_counts[]  /* in */, 
        int    bin_count     /* in */, 
        float  min_meas      /* in */) {
   int i, j;
   float bin_max, bin_min;

   for (i = 0; i < bin_count; i++) {
      bin_max = bin_maxes[i];
      bin_min = (i == 0) ? min_meas: bin_maxes[i-1];
      printf("%.3f-%.3f:\t", bin_min, bin_max);
      for (j = 0; j < bin_counts[i]; j++)
         printf("X");
      printf("\n");
   }
}  /* Print_histo */

/*
 * the method run by each thread, will sort the generated data into bins
 */
void *Thread_func(void *rank) {
	long my_rank = (long) rank;
	int* loc_bin_counts = malloc(bin_count * sizeof(int));
	int i, bin;
	int chunk_size = data_count/thread_count;
	
	for (i=0; i<bin_count; i++) { //initialize all to 0
		loc_bin_counts[i] = 0;
	}
	
   /* Count number of values in each bin */
   for (i = (my_rank*chunk_size); i < ((my_rank+1)*chunk_size); i++) {
      bin = Which_bin(data[i], bin_maxes, bin_count, min_meas);
      loc_bin_counts[bin]++;
   }
   
   //for loop adding to global bin
   for (i=0; i< bin_count; i++) {
      pthread_mutex_lock(&locks[i]);
	  bin_counts[i] += loc_bin_counts[i];
	  pthread_mutex_unlock(&locks[i]);
   }
   
   Barrier();
   
   if (my_rank == 0) {
      /* Print the histogram */
      Print_histo(bin_maxes, bin_counts, bin_count, min_meas);
   }
   
   free(loc_bin_counts);
   return NULL;
}

void Barrier() {
   /* Barrier */
   pthread_mutex_lock(&barrier_lock);
   barrier_counter++;
   if (barrier_counter == thread_count) {
      barrier_counter = 0;
      pthread_cond_broadcast(&barrier_cond);
   } 
   else {
      while (pthread_cond_wait(&barrier_cond, &barrier_lock) != 0);
   }
   pthread_mutex_unlock(&barrier_lock);
}
