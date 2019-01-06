#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>


/* Function that returns the average of 4 doubles */
double average(double a, double b, double c, double d) {
    return (a + b + c + d) / 4.0;
}

/* Function that prints a given array with a given dimension */
void printArray(int dimension, double **current) {
    int i, j;
    for (i = 0; i < dimension; i++) {
        for (j = 0; j < dimension; j++) {
            printf("%lf ", current[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

/* Function that reads in the array of a given size from an open file.
 * Makes a copy of this array to be used as the next array */
void setArray(int dimension, double **one, double **two, FILE * fp) {
    int i, j;
    char pwd[100];
    for (i = 0; i < dimension; i++){
        for (j = 0; j < dimension; j++) {
            fscanf(fp, "%lf,", &one[i][j]);
            two[i][j] = one[i][j];
        }
    }
}

/* Function that sets the values of the start and end index of the array that each thread has to work on.
 * Does the calculation to split the array and the remainder rows. */
void setThreadArraySections(int *start_i, int *end_i, int dimension, int numProcesses, int self){
    int remainder, rowsToUse, numRows;

    // idk why i started thread ids with 1 not 0
    self +=1;
    numRows = dimension - 2;
    rowsToUse = (int) floor(numRows / numProcesses);
    remainder = (numRows % numProcesses);
    if (remainder - self >= 0) {
        *start_i = (self - 1) * rowsToUse + (self - 1) + 1;
        rowsToUse++;
    } else {
        *start_i = (self - 1) * rowsToUse + remainder + 1;
    }
    *end_i = *start_i + rowsToUse;
}

/* Runs the sequential algorithm on a given array in the args_struct */
void sequentialSolver(int dimension, double **currentArray, double **nextArray, double precision) {
    double a, b, c, d, av, current, diff;
    int i, j;
    int stop = 0;
    int count = 0;
    int run = 0;
    double **readArray = currentArray;
    double **writeArray = nextArray;

    while (stop == 0) {
        stop = 1;
        for (i = 1; i < dimension - 1; i++) {
            for (j = 1; j < dimension - 1; j++) {
                current = readArray[i][j];
                a = readArray[i - 1][j];
                b = readArray[i][j - 1];
                c = readArray[i + 1][j];
                d = readArray[i][j + 1];
                av = average(a, b, c, d);
                diff = fabs(av - current);
                writeArray[i][j] = av;
                if (stop == 1 & diff > precision) {
                    stop = 0;
                }
            }
        }
        if (count == 0) {
            readArray = nextArray;
            writeArray = currentArray;
            count++;
        }
        else {
            readArray = currentArray;
            writeArray = nextArray;
            count--;
        }
//        run++;
    }
//    printf("Sequential runs: %d\n", run);
}

/* Runs the sequential algorithm on a given array in the args_struct */
void mpiParallel(int dimension, int rows, int arraySize, double **currentArray, double **nextArray, double precision, int rank, int numProcesses) {
    double a, b, c, d, av, current, diff;
    int i, j;
    int stop = 0;
    int count = 0;
    int run = 0;
    double **readArray = currentArray;
    double **writeArray = nextArray;

    int k, l;

    while (1) {
//    while (run != 1) {
        stop = 1;
        for (i = 1; i < arraySize - 1; i++) {
            for (j = 1; j < dimension - 1; j++) {
                current = readArray[i][j];
                a = readArray[i - 1][j];
                b = readArray[i][j - 1];
                c = readArray[i + 1][j];
                d = readArray[i][j + 1];
                av = average(a, b, c, d);
                diff = fabs(av - current);
                writeArray[i][j] = av;
                if (stop == 1 & diff > precision) {
                    stop = 0;
                }
//                printf("Stop %d rank %d diff %lf precision %lf\n", stop, rank, diff, precision);
            }
        }

        // barrier
        MPI_Barrier(MPI_COMM_WORLD);

        // broadcast stop variable
        int process;
        int broadcastStop = 0;
        for (process = 0; process<numProcesses; process++){
            if (rank == process) {
                broadcastStop = stop;
            }
            MPI_Bcast(&broadcastStop, 1, MPI_INT, process, MPI_COMM_WORLD);
            if (broadcastStop == 0) {
                printf("continue rank %d\n", rank);
                break;
            }
        }

        // check stop
        if (broadcastStop == 1) {
            printf("Stop rank %d\n", rank);
            break;
        }

        // send neighbour rows
        MPI_Request req[4];
        MPI_Status recvStat[2];

        // send above neighbour (rank - 1)
        // check for top process
        if (rank > 0){
            MPI_Isend(writeArray[1], dimension, MPI_DOUBLE, rank - 1, 98, MPI_COMM_WORLD, &req[0]);
//            for (l = 0; l < dimension; l++) {
//                printf("rank: %d send to: %d index: %d value: %lf\n", rank, rank-1, l, writeArray[1][l]);
//            }
        }

        // send below neighbour (rank + 1)
        // check for bottom
        if (rank < numProcesses - 1) {
            MPI_Isend(writeArray[rows], dimension, MPI_DOUBLE, rank + 1, 99, MPI_COMM_WORLD, &req[1]);
//            for (l = 0; l < dimension; l++) {
//                printf("rank: %d send to: %d index: %d value: %lf\n", rank, rank+1, l, writeArray[rows][l]);
//                printf("%lf ", writeArray[arraySize-2][l]);
//            }
//            printf("\n");
        }

        // receive above neighbour (rank - 1)
        // check for top process
        if (rank  > 0){
            MPI_Irecv(writeArray[0], dimension, MPI_DOUBLE, rank - 1, 99, MPI_COMM_WORLD, &req[2]);
        }


        // receive below neighbour (rank + 1)
        // check for bottom
        if (rank < numProcesses - 1) {
            MPI_Irecv(writeArray[rows+1], dimension, MPI_DOUBLE, rank + 1, 98, MPI_COMM_WORLD, &req[3]);
        }

        // wait for receive
        if (rank - 1 >= 0) {
            MPI_Wait(&req[2], &recvStat[0]);
//            for (l = 0; l < dimension; l++) {
//                printf("rank: %d recv from: %d index: %d value: %lf\n", rank, rank-1, l, writeArray[0][l]);
//            }
//            printf("\n");
        }


        if (rank + 1 < numProcesses) {
            MPI_Wait(&req[3], &recvStat[1]);
//            for (l = 0; l < dimension; l++) {
//                printf("rank: %d recv from: %d index: %d value: %lf\n", rank, rank+1, l, writeArray[rows+1][l]);
//            }
//            printf("\n");
        }


//        printf("After Receive: rank = %d\n", rank);
//        for(k = 0; k < arraySize; k++) {
//            for (l = 0; l < dimension; l++) {
//                printf("%lf ", nextArray[k][l]);
//            }
//            printf("\n");
//        }
//        MPI_Barrier(MPI_COMM_WORLD);
//        break;


        if (count == 0) {
            readArray = nextArray;
            writeArray = currentArray;
            count++;
        }
        else {
            readArray = currentArray;
            writeArray = nextArray;
            count--;
        }
    }
}

/* Checks if the difference between the final two arrays is less than the precision */
int precisionTest(int dimension, double **currentArray, double **nextArray, double precision) {
    int count = 0;
    double diff;
    int i, j;
    for (i = 0;i <dimension; i++) {
        for (j = 0;j <dimension; j++) {
            diff = fabs(currentArray[i][j] - nextArray[i][j]);
            if (diff > precision) {
                count++;
            }
        }
    }
    return count;
}

/* Checks if there is a difference between the sequential result array and the parallel result arg_struct */
void correctnessTest(int dimension, double **seqArray, double **parArray) {
    int k;
    double **results;
    results = (double**)malloc(sizeof(double*) * dimension);
    for(k = 0; k < dimension; k++) {
        results[k] = (double*)malloc(sizeof(double) * dimension);
    }
    int count = 0;
    double diff;
    int i, j;

    for (i = 0; i < dimension; i++) {
        for (j = 0; j < dimension; j++) {
            diff = fabs(seqArray[i][j] - parArray[i][j]);
            if (diff > 0.000001) {
                results[i][j] = diff;
                count++;
            }
        }
    }
    if(count != 0){
        printf("Not correct in %d places\n", count);
        printArray(dimension, results);
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
void runSequential(int dimension, double **currentArray, double **nextArray, double precision, char * filename) {
    double begin, end;
    long time_spent;
    begin = MPI_Wtime();
    sequentialSolver(dimension, currentArray, nextArray, precision);
    end = MPI_Wtime();

    time_spent = (long)((end - begin) * 1000000000L);

    FILE * fpWrite = fopen(filename, "w+");
    fprintf(fpWrite, "Time: %llu nanoseconds.\n", (unsigned long long int)time_spent);
    int count = precisionTest(dimension, currentArray, nextArray, precision);
    if(count != 0){
        fprintf(fpWrite, "Not precise in %d places\n", count);
    }
    else {
        fprintf(fpWrite, "Precise\n");
    }
    int i, j;
    for (i = 0; i < dimension; i++) {
        for (j = 0; j < dimension; j++) {
            fprintf(fpWrite, "%lf,", currentArray[i][j]);
        }
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
    int numProcesses;
    int rank;
    int dimension = atoi(argv[1]);
    double precision = atof(argv[2]);

    // Initialise MPI
    MPI_Init(NULL, NULL);

    // Get number of processes
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);
    // Get individual process rank
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int row_start = 0;
    int row_end = 0;
    setThreadArraySections(&row_start, &row_end, dimension, numProcesses, rank);
    int rows = row_end - row_start;
    int arraySize = rows+2;

    printf("rec rank: %d, start: %d, end: %d, arraySize: %d\n", rank, row_start, row_end, arraySize);

    double **seqCurrentArray;
    double **seqNextArray;
    double **parCurrentArray;
    double **parNextArray;
    double **currentArray;
    double **nextArray;
    // if main process allocate whole array for processing accuracy and precision later
    // otherwise allocate what you need
    int i,j;
    if (rank==0) {
        parCurrentArray = (double**)malloc(sizeof(double*) * dimension);
        parNextArray = (double**)malloc(sizeof(double*) * dimension);
        for(j = 0; j < dimension; j++) {
            parCurrentArray[j] = (double*)malloc(sizeof(double) * dimension);
            parNextArray[j] = (double*)malloc(sizeof(double) * dimension);
        }
    }
    currentArray = malloc(sizeof(double) * (arraySize));
    nextArray = malloc(sizeof(double) * (arraySize));
    for(j = 0; j < arraySize; j++) {
        currentArray[j] = malloc(sizeof(double) * (dimension));
        nextArray[j] = malloc(sizeof(double) * (dimension));
    }

    char seqFilename[128];
    double beginTime, endTime, time_spent;

    // rank 0 = master process
    if (rank == 0) {
        // only run sequential check from master thread
        sprintf(seqFilename, "/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/seqOut_%d.txt", dimension);
        if (numProcesses == 1) {
            seqCurrentArray = (double**)malloc(sizeof(double*) * dimension);
            seqNextArray = (double**)malloc(sizeof(double*) * dimension);
            for(j = 0; j < dimension; j++) {
                seqCurrentArray[j] = (double*)malloc(sizeof(double) * dimension);
                seqNextArray[j] = (double*)malloc(sizeof(double) * dimension);
            }
            FILE * fpRead = fopen("/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/numbers.txt", "r+");
            char pwd[100];
            setArray(dimension, seqCurrentArray, seqNextArray, fpRead);
            fclose(fpRead);

            runSequential(dimension, seqCurrentArray, seqNextArray, precision, seqFilename);
        }
            // else send processes rows
        else {
            FILE * fp1 = fopen("/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/numbers.txt", "r+");
            setArray(dimension, parCurrentArray, parNextArray, fp1);
            fclose(fp1);
            MPI_Request sendReq[2];
            int processRowStart, processRowEnd;
            for (i = 1; i < numProcesses; i ++) {
                setThreadArraySections(&processRowStart, &processRowEnd, dimension, numProcesses, i);
                for(j = processRowStart - 1; j < processRowEnd + 1; j++) {
                    MPI_Isend(parCurrentArray[j], dimension, MPI_DOUBLE, i, processRowStart, MPI_COMM_WORLD, &sendReq[0]);
                    MPI_Isend(parNextArray[j], dimension, MPI_DOUBLE, i, processRowStart, MPI_COMM_WORLD, &sendReq[1]);
                }
            }
            for(j = row_start - 1; j < row_end + 1; j++) {
                currentArray[j] = parCurrentArray[j];
                nextArray[j] = parNextArray[j];
            }
        }
    }
    else {
        MPI_Request recvReq[2];
        MPI_Status recvStat[2];
        for(j = 0; j < arraySize; j++) {
            MPI_Irecv(currentArray[j], dimension, MPI_DOUBLE, 0, row_start, MPI_COMM_WORLD, &recvReq[0]);
            MPI_Irecv(nextArray[j], dimension, MPI_DOUBLE, 0, row_start, MPI_COMM_WORLD, &recvReq[1]);
            MPI_Wait(&recvReq[0], &recvStat[0]);
            MPI_Wait(&recvReq[1], &recvStat[1]);
        }
    }

    // receive rows

//    printf("test rank %d\n", rank);
//    for(i = 0; i < arraySize; i++){
//        for(j=0; j<dimension; j++){
//            printf("%lf ", currentArray[i][j]);
//        }
//        printf("\n");
//    }

    // barrier
    MPI_Barrier(MPI_COMM_WORLD);

    // Start parallel
    if (rank == 0) {
        beginTime = MPI_Wtime();
    }

//    if (rank == 0) {
//        currentArray[0][0] = rank;
//    }
//    if (rank == 1) {
//        currentArray[1][0] = rank + 3;
//    }

    if (numProcesses != 1) {
        mpiParallel(dimension, rows, arraySize, currentArray, nextArray, precision, rank, numProcesses);
    }
    printf("end parallel\n\n");


    // end parallel
    if (rank == 0) {
        endTime = MPI_Wtime();
    }

    // send back all rows
    if (rank > 0){
        MPI_Request sendReq[2];
        for(j = 0; j < arraySize; j++) {
            MPI_Isend(currentArray[j], dimension, MPI_DOUBLE, 0, row_start, MPI_COMM_WORLD, &sendReq[0]);
            MPI_Isend(nextArray[j], dimension, MPI_DOUBLE, 0, row_start, MPI_COMM_WORLD, &sendReq[1]);
        }
    }
    else {
        time_spent = (long)((endTime - beginTime) * 1000000000L);
        MPI_Request endRecvReq[2];
        MPI_Status endRecvStat[2];
        for (i = 1; i < numProcesses; i ++) {
            setThreadArraySections(&row_start, &row_end, dimension, numProcesses, i);
            for(j = row_start - 1; j < row_end + 1; j++) {
                MPI_Irecv(parCurrentArray[j], dimension, MPI_DOUBLE, i, row_start, MPI_COMM_WORLD, &endRecvReq[0]);
                MPI_Irecv(parNextArray[j], dimension, MPI_DOUBLE, i, row_start, MPI_COMM_WORLD, &endRecvReq[1]);
                MPI_Wait(&endRecvReq[0], &endRecvStat[0]);
                MPI_Wait(&endRecvReq[1], &endRecvStat[1]);
            }
        }
//        printArray(dimension, parCurrentArray);


        // read sequential result from file
        double **readSeqCurrentArray;
        double **readSeqNextArray;
        readSeqCurrentArray = (double**)malloc(sizeof(double*) * dimension);
        readSeqNextArray = (double**)malloc(sizeof(double*) * dimension);
        for(j = 0; j < dimension; j++) {
            readSeqCurrentArray[j] = (double*)malloc(sizeof(double) * dimension);
            readSeqNextArray[j] = (double*)malloc(sizeof(double) * dimension);
        }

        char firstLine[64];
        char precise[40];
        FILE * fpReadSeqArray = fopen(seqFilename, "r+");
        fgets(firstLine, 64, fpReadSeqArray);
        fgets(precise, 64, fpReadSeqArray);
        setArray(dimension, readSeqCurrentArray, readSeqNextArray, fpReadSeqArray);
        fclose(fpReadSeqArray);

        // Precision and Correctness tests
        printf("Array size %d by %d\n", dimension, dimension);
        printf("Sequential run\n");
        printf("%s", firstLine);
        printf("%s", precise);
        printf("Parallel run\n");
        printf("Number of Threads: %d\n", numProcesses);
        printf("Time: %llu nanoseconds.\n", (unsigned long long int)time_spent);
        int count = precisionTest(dimension, readSeqCurrentArray, parCurrentArray, precision);
        if (count != 0) {
            printf("Not precise in %d places\n", count);
        } else {
            printf("Precise\n");
        }
        correctnessTest(dimension, readSeqCurrentArray, parCurrentArray);
    }

    MPI_Finalize();
    return 0;
}
