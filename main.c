#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>

pthread_mutex_t lock;
pthread_barrier_t barrier;

/* Struct that stores all of the variables that are shared between the threads */
struct arg_struct {
    int dimension;
    double *currentArray;
    double *nextArray;
    double precision;
    int numThreads;
    int *currentStop;
    int *nextStop;
};

/* Struct that stores a pointer to an args_struct and the int thread id for each thread */
struct thread_arg {
    int threadId;
    struct arg_struct * args;
};

/* Function that returns the average of 4 doubles */
double average(double a, double b, double c, double d) {
    return (a + b + c + d) / 4.0;
}

/* Function that prints a given array with a given dimension */
void printArray(const double *current, int dimension) {
    int i, j;
    for (i = 0; i < dimension; i++) {
        for (j = 0; j < dimension; j++) {
            printf("%lf ", current[i * dimension + j]);
        }
        printf("\n");
    }
}

/* Function that reads in the array of a given size from an open file.
 * Makes a copy of this array to be used as the next array */
void setArray(double *one, double *two, int size, FILE * fp) {
    int k;
    char pwd[100];
    for (k = 0; k < size; k++){
        fscanf(fp, "%lf,", &one[k]);
        two[k] = one[k];
    }
}

/* Function that sets the values of the start and end index of the array that each thread has to work on.
 * Does the calculation to split the array and the remainder rows. */
void setThreadArraySections(int *start_i, int *end_i, int dimension, int numThreads, int self){
    int remainder, rowsToUse, numRows;

    numRows = dimension - 2;
    rowsToUse = (int) floor(numRows / numThreads);
    remainder = (numRows % numThreads);
    if (remainder - self >= 0) {
        *start_i = (self - 1) * rowsToUse + (self - 1) + 1;
        rowsToUse++;
    } else {
        *start_i = (self - 1) * rowsToUse + remainder + 1;
    }
    *end_i = *start_i + rowsToUse;
}

/* Runs the sequential algorithm on a given array in the args_struct */
void sequentialSolver(void *arguments) {
    struct arg_struct *args = (struct arg_struct *) arguments;
    double a, b, c, d, av, current, diff;
    int i, j;
    int stop = 0;
    int count = 0;
    int run = 0;
    double *localOne = args->currentArray;
    double *localTwo = args->nextArray;

    while (stop == 0) {
        stop = 1;
        for (i = 1; i < args->dimension - 1; i++) {
            for (j = 1; j < args->dimension - 1; j++) {
                current = localOne[i * args->dimension + j];
                a = *((localOne + (i - 1) * args->dimension) + j);
                b = *((localOne + i * args->dimension) + (j - 1));
                c = *((localOne + (i + 1) * args->dimension) + j);
                d = *((localOne + i * args->dimension) + (j + 1));
                av = average(a, b, c, d);
                diff = fabs(av - current);
                localTwo[i * args->dimension + j] = av;
                if (stop == 1 && diff > args->precision) {
                    stop = 0;
                }
            }
        }
        run++;
        if (count == 0) {
            localOne = args->nextArray;
            localTwo = args->currentArray;
            count++;
        }
        else {
            localOne = args->currentArray;
            localTwo = args->nextArray;
            count--;
        }
    }
//    printf("Sequential runs: %d\n", run);
}

/* Runs the parallelised algorithm for a given thread on a given array in the thread_arg  */
void *parallelSolver(void *arguments) {
    struct thread_arg *thread_args = (struct thread_arg *) arguments;
    struct arg_struct *args = thread_args->args;

    int i, j;
    int count = 0;
    int self = thread_args->threadId;
    int start_i = 0;
    int end_i = 0;
    int run = 0;
    int *localStopOne = args->currentStop;
    int *localStopTwo = args->nextStop;
    double *localOne = args->currentArray;
    double *localTwo = args->nextArray;
    double a, b, c, d, av, current, diff;

    setThreadArraySections(&start_i, &end_i, args->dimension, args->numThreads, self);

    while (1) {
        for (i = start_i; i < end_i; i++) {
            for (j = 1; j < args->dimension - 1; j++) {
                current = localOne[i * args->dimension + j];
                a = *((localOne + (i - 1) * args->dimension) + j);
                b = *((localOne + i * args->dimension) + (j - 1));
                c = *((localOne + (i + 1) * args->dimension) + j);
                d = *((localOne + i * args->dimension) + (j + 1));
                av = average(a, b, c, d);
                diff = fabs(av - current);
                localTwo[i * args->dimension + j] = av;
                if (*localStopOne == 1 && diff > args->precision) {
                    pthread_mutex_lock(&lock);
                    *localStopOne = 0;
                    pthread_mutex_unlock(&lock);
                }
            }
        }
        pthread_barrier_wait(&barrier);
        if (self == 1) {
            run++;
        }
        if (*localStopOne == 1) {
//            if (self == 1) {
//                printf("Parallel runs: %d\n", run);
//            }
            pthread_exit(0);
            break;
        }
        if (*localStopTwo != 1) {
            pthread_mutex_lock(&lock);
            *localStopTwo = 1;
            pthread_mutex_unlock(&lock);
        }
        pthread_barrier_wait(&barrier);
        if (count == 0) {
            localOne = args->nextArray;
            localTwo = args->currentArray;
            localStopOne = args->currentStop;
            localStopTwo = args->nextStop;
            count++;
        }
        else {
            localOne = args->currentArray;
            localTwo = args->nextArray;
            localStopOne = args->nextStop;
            localStopTwo = args->currentStop;
            count--;
        }
    }
}

/* Checks if the difference between the final two arrays is less than the precision */
int precisionTest(void *three) {
    struct arg_struct *args = (struct arg_struct *) three;
    int size = args->dimension*args->dimension;
    int count = 0;
    double diff;
    int i;
    for (i = 0;i <size; i++) {
        diff = fabs(args->currentArray[i] - args->nextArray[i]);
        if (diff > args->precision) {
            count++;
        }
    }
    return count;
}

/* Checks if there is a difference between the sequential result array and the parallel result arg_struct */
void correctnessTest(double seqArray[], void * two) {
    struct arg_struct *args2 = (struct arg_struct *) two;
    int size = args2->dimension*args2->dimension;
    double results[size];
    memset(results, 0, sizeof(results));
    int count = 0;
    double diff;
    int i, j;

    for (i = 0; i < args2->dimension; i++) {
        for (j = 0; j < args2->dimension; j++) {
            diff = fabs(seqArray[i * args2->dimension + j] - args2->currentArray[i * args2->dimension + j]);
            if (diff > 0.000001) {
                results[i * args2->dimension + j] = diff;
                count++;
            }
        }
    }
    if(count != 0){
        printf("Not correct in %d places\n", count);
        printArray(results, args2->dimension);
    }
    else {
        printf("Correct\n");
    }
}

/* Runs the sequential algorithm for a given array and outputs:
 *  - the time taken
 *  - the result of the precision check
 *  - the final array
 *  to a file with a given filename
 */
void runSequential(double arr3[], double arr4[], int dimension, double precision, int numThreads, char * filename) {
    int arraySize = dimension * dimension;

    struct timespec begin, end;
    time_t time_spent;

    struct arg_struct seqArgs;
    seqArgs.dimension = dimension;
    seqArgs.precision = precision;
    seqArgs.currentArray = arr3;
    seqArgs.nextArray = arr4;
    seqArgs.numThreads = numThreads;
    seqArgs.currentStop = malloc(sizeof(int));
    seqArgs.nextStop = malloc(sizeof(int));

    clock_gettime(CLOCK_MONOTONIC, &begin);
    sequentialSolver(&seqArgs);
    clock_gettime(CLOCK_MONOTONIC, &end);

    time_spent = (1000000000L * (end.tv_sec - begin.tv_sec)) + end.tv_nsec - begin.tv_nsec;
    FILE * fpWrite = fopen(filename, "w+");
    fprintf(fpWrite, "Time: %llu nanoseconds.\n", (unsigned long long int) time_spent);
    int count = precisionTest(&seqArgs);
    if(count != 0){
        fprintf(fpWrite, "Not precise in %d places\n", count);
    }
    else {
        fprintf(fpWrite, "Precise\n");
    }
    int i;
    for (i = 0; i < arraySize; i++) {
        fprintf(fpWrite, "%lf,", seqArgs.currentArray[i]);
    }
    fclose(fpWrite);
}

/* Main method that has 3 parameters:
 *  - number of threads (int)
 *  - array dimension
 *  - precision
 *  Fills an array of size dimension squared with values read in from the file of comma separated
 *  numbers
 *  Runs sequential with that array if number of threads is 1
 *  Runs parallel and checks the correctness by reading in the sequential result for that array dimension */
int main(int argc, char *argv[]) {
    if (argc > 1) {
        int numThreads = atoi(argv[1]);
        int dimension = atoi(argv[2]);
        double precision = atof(argv[3]);
        int arraySize = dimension*dimension;

        struct timespec begin, end;
        time_t time_spent;

        double arr1[arraySize];
        double arr2[arraySize];
        double arr3[arraySize];
        double arr4[arraySize];
        FILE * fp1 = fopen("numbers.txt", "r+");
        setArray(arr1, arr2, arraySize, fp1);
        fclose(fp1);

        char seqFilename[64];
        sprintf(seqFilename, "seqOut_%d.txt", dimension);
        if (numThreads == 1) {
            FILE * fpRead = fopen("numbers.txt", "r+");
            setArray(arr3, arr4, arraySize, fpRead);
            fclose(fpRead);

            runSequential(arr3, arr4, dimension, precision, numThreads, seqFilename);
        }

        double seqArray1[arraySize];
        double seqArray2[arraySize];
        char firstLine[64];
        char precise[40];
        FILE * fpReadSeqArray = fopen(seqFilename, "r+");
        fgets(firstLine, 64, fpReadSeqArray);
        fgets(precise, 64, fpReadSeqArray);
        setArray(seqArray1, seqArray2, arraySize, fpReadSeqArray);
        fclose(fpReadSeqArray);

        clock_gettime(CLOCK_MONOTONIC, &begin);
        pthread_t *thread = malloc(sizeof(pthread_t) * (long unsigned int) numThreads);
        if (thread == NULL) {
            printf("out of memory\n");
            exit(EXIT_FAILURE);
        }

        if (pthread_mutex_init(&lock, NULL) != 0) {
            printf("mutex init failed\n");
            exit(EXIT_FAILURE);
        }

        if (pthread_barrier_init(&barrier, NULL, (unsigned int) numThreads) != 0) {
            printf("barrier init failed\n");
            exit(EXIT_FAILURE);
        }

        struct arg_struct parallelSharedArgs;
        parallelSharedArgs.dimension = dimension;
        parallelSharedArgs.precision = precision;
        parallelSharedArgs.currentArray = arr1;
        parallelSharedArgs.nextArray = arr2;
        parallelSharedArgs.numThreads = numThreads;
        parallelSharedArgs.currentStop = malloc(sizeof(int));
        parallelSharedArgs.nextStop = malloc(sizeof(int));
        struct thread_arg parallelArgs[numThreads];

        int i;
        for (i = 0; i < numThreads; i++) {
            int *id = malloc(sizeof(int));
            *id = i+1;
            parallelArgs[i].args = &parallelSharedArgs;
            parallelArgs[i].threadId = *id;
            if (pthread_create(&thread[i], NULL, parallelSolver, (void *) &parallelArgs[i]) != 0) {
                printf("Error. \n");
                exit(EXIT_FAILURE);
            }
        }
        for (i = 0; i < numThreads; i++) {
            pthread_join(thread[i], NULL);
        }
        pthread_mutex_destroy(&lock);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_spent = (1000000000L * (end.tv_sec - begin.tv_sec)) + end.tv_nsec - begin.tv_nsec;

        printf("Array size %d by %d\n", dimension, dimension);
        printf("Sequential run\n");
        printf("%s", firstLine);
        printf("%s", precise);
        printf("Parallel run\n");
        printf("Number of Threads: %d\n", numThreads);
        printf("Time: %llu nanoseconds.\n", (unsigned long long int) time_spent);
        int count = precisionTest(&parallelSharedArgs);
        if(count != 0){
            printf("Not precise in %d places\n", count);
        }
        else {
            printf("Precise\n");
        }
        correctnessTest(seqArray1, &parallelSharedArgs);
    }
    return 0;
}
