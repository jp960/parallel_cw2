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
void printArray(int dimension, const double current[dimension][dimension]) {
    int i, j;
    for (i = 0; i < dimension; i++) {
        for (j = 0; j < dimension; j++) {
            printf("%lf ", current[i][j]);
        }
        printf("\n");
    }
}

/* Function that reads in the array of a given size from an open file.
 * Makes a copy of this array to be used as the next array */
void setArray(int dimension, double one[dimension][dimension], double two[dimension][dimension], FILE * fp) {
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
void sequentialSolver(int dimension, double currentArray[dimension][dimension], double nextArray[dimension][dimension], double precision) {
    double a, b, c, d, av, current, diff;
    int i, j;
    int stop = 0;
    int count = 0;
    int run = 0;
    double *localOne = &currentArray[0][0];
    double *localTwo = &nextArray[0][0];

    while (stop == 0) {
        stop = 1;
        for (i = 1; i < dimension - 1; i++) {
            for (j = 1; j < dimension - 1; j++) {
                current = localOne[i * dimension + j];
                a = *((localOne + (i - 1) * dimension) + j);
                b = *((localOne + i * dimension) + (j - 1));
                c = *((localOne + (i + 1) * dimension) + j);
                d = *((localOne + i * dimension) + (j + 1));
                av = average(a, b, c, d);
                diff = fabs(av - current);
                localTwo[i * dimension + j] = av;
                if (stop == 1 & diff > precision) {
                    stop = 0;
                }
            }
        }
        run++;
        if (count == 0) {
            localOne = &nextArray[0][0];
            localTwo = &currentArray[0][0];
            count++;
        }
        else {
            localOne = &currentArray[0][0];
            localTwo = &nextArray[0][0];
            count--;
        }
    }
//    printf("Sequential runs: %d\n", run);
}

/* Runs the sequential algorithm on a given array in the args_struct */
void mpiParallel(int dimension, int start, int end, int arraySize, double currentArray[arraySize][dimension], double nextArray[arraySize][dimension], double precision, int rank, int numProcesses) {
    double a, b, c, d, av, current, diff;
    int i, j;
    int stop = 0;
    int count = 0;
    int run = 0;
    double *localOne = &currentArray[0][0];
    double *localTwo = &nextArray[0][0];

//    printf("Before: rank = %d, start = %d, end = %d, arraySize = %d\n", rank, start, end, arraySize);
    int k, l;
//    for(k = 0; k < arraySize; k++) {
//        for (l = 0; l < dimension; l++) {
//            printf("%lf ", nextArray[k][l]);
//        }
//        printf("\n");
//    }

//    while (1) {
    while (run != 1) {
        stop = 1;
        for (i = 1; i < arraySize - 1; i++) {
            for (j = 1; j < dimension - 1; j++) {
                current = localOne[i * dimension + j];
                a = *((localOne + (i - 1) * dimension) + j);
                b = *((localOne + i * dimension) + (j - 1));
                c = *((localOne + (i + 1) * dimension) + j);
                d = *((localOne + i * dimension) + (j + 1));
                av = average(a, b, c, d);
                diff = fabs(av - current);
                localTwo[i * dimension + j] = av;
                if (stop == 1 & diff > precision) {
                    stop = 0;
                }
//                printf("Stop %d rank %d diff %lf precision %lf\n", stop, rank, diff, precision);
            }
        }

        // barrier
        MPI_Barrier(MPI_COMM_WORLD);

        // broadcast stop variable
//        int process;
//        int broadcastStop = 0;
//        for (process = 0; process<numProcesses; process++){
//            if (rank == process) {
//                broadcastStop = stop;
//            }
//            MPI_Bcast(&broadcastStop, 1, MPI_INT, process, MPI_COMM_WORLD);
//            if (broadcastStop == 0) {
//                printf("continue rank %d\n", rank);
//                break;
//            }
//        }
//
//        // check stop
//        if (broadcastStop == 1) {
//            printf("Stop rank %d\n", rank);
//            break;
//        }

        // send neighbour rows
        MPI_Request req[4];
        MPI_Status recvStat[2];

        // send above neighbour (rank - 1)
        // check for top process
        if (rank - 1 >= 0){
//            printf("sending up row %d from rank %d to rank-1 %d with tag %d\n", 1, rank, rank-1, 1);
            MPI_Isend(&localTwo[1], dimension, MPI_DOUBLE, rank - 1, 1, MPI_COMM_WORLD, &req[0]);
            printf("row send up: from rank %d to rank-1 %d with tag %d\n", rank, rank-1, 1);
            for (l = 0; l < dimension; l++) {
                printf("%lf ", localTwo[1*dimension+l]);
            }
            printf("\n");
        }

        // send below neighbour (rank + 1)
        // check for bottom
        if (rank + 1 < numProcesses) {
//            printf("sending down row %d from rank %d to rank+1 %d with tag %d\n", arraySize - 2 , rank, rank+1, 2);
            MPI_Isend(&localTwo[arraySize - 2], dimension, MPI_DOUBLE, rank + 1, 2, MPI_COMM_WORLD, &req[1]);
            printf("row send down: from rank %d to rank-1 %d with tag %d\n", rank, rank+1, 1);
            for (l = 0; l < dimension; l++) {
                printf("%lf ", localTwo[(arraySize-2)*dimension+l]);
            }
            printf("\n");
        }

        // receive above neighbour (rank - 1)
        // check for top process
        if (rank - 1 >= 0){
//            printf("receiving up row %d from rank-1 %d to rank %d with tag %d\n", 0, rank-1, rank, 1);
            printf("before row receive up: from rank %d to rank-1 %d with tag %d\n", rank, rank-1, 2);
            for (l = 0; l < dimension; l++) {
                printf("%lf ", localTwo[0*dimension+l]);
            }
            printf("\n");
            MPI_Irecv(&localTwo[0], dimension, MPI_DOUBLE, rank - 1, 2, MPI_COMM_WORLD, &req[2]);
//            MPI_Irecv(&arrayTest1[0], dimension, MPI_DOUBLE, rank - 1, 2, MPI_COMM_WORLD, &req[2]);
        }


        // receive below neighbour (rank + 1)
        // check for bottom
        if (rank + 1 < numProcesses) {
//            printf("receiving down row %d from rank+1 %d to rank %d with tag %d\n", arraySize - 1 , rank+1, rank, 2);
            printf("before row receive down: from rank %d to rank+1 %d with tag %d\n", rank, rank+1, 1);
            for (l = 0; l < dimension; l++) {
                printf("%lf ", localTwo[(arraySize-1)*dimension+l]);
            }
            printf("\n");
            MPI_Irecv(&localTwo[arraySize-1], dimension, MPI_DOUBLE, rank + 1, 1, MPI_COMM_WORLD, &req[3]);
//            MPI_Irecv(&arrayTest2[0], dimension, MPI_DOUBLE, rank + 1, 1, MPI_COMM_WORLD, &req[3]);
        }

        // wait for receive
        if (rank - 1 >= 0) {
            MPI_Wait(&req[2], &recvStat[0]);
            printf("row receive up: from rank %d to rank-1 %d with tag %d\n", rank, rank-1, 2);
            for (l = 0; l < dimension; l++) {
                printf("%lf ", localTwo[0*dimension+l]);
            }
            printf("\n");
        }


        if (rank + 1 < numProcesses) {
            MPI_Wait(&req[3], &recvStat[1]);
            printf("row receive down: from rank %d to rank+1 %d with tag %d\n", rank, rank+1, 1);
            for (l = 0; l < dimension; l++) {
                printf("%lf ", localTwo[(arraySize-1)*dimension+l]);
            }
            printf("\n");
        }


//        printf("After Receive: rank = %d\n", rank);
//        for(k = 0; k < arraySize; k++) {
//            for (l = 0; l < dimension; l++) {
//                printf("%lf ", nextArray[k][l]);
//            }
//            printf("\n");
//        }
        MPI_Barrier(MPI_COMM_WORLD);
//        break;


        if (count == 0) {
            localOne = &nextArray[0][0];
            localTwo = &currentArray[0][0];
            count++;
        }
        else {
            localOne = &currentArray[0][0];
            localTwo = &nextArray[0][0];
            count--;
        }
        run++;
//        printf("rank %d run %d\n", rank, run);
    }
}

/* Checks if the difference between the final two arrays is less than the precision */
int precisionTest(int dimension, double currentArray[dimension][dimension], double nextArray[dimension][dimension], double precision) {
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
void correctnessTest(int dimension, double seqArray[dimension][dimension], double parArray[dimension][dimension]) {
    double results[dimension][dimension];
    memset(results, 0, sizeof(results));
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
void runSequential(int dimension, double currentArray[dimension][dimension], double nextArray[dimension][dimension], double precision, char * filename) {
    double begin, end, time_spent;
    begin = MPI_Wtime();
    sequentialSolver(dimension, currentArray, nextArray, precision);
    end = MPI_Wtime();

    time_spent = end - begin;
    FILE * fpWrite = fopen(filename, "w+");
    fprintf(fpWrite, "Time: %lf seconds.\n", time_spent);
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

    char seqFilename[128];
    double beginTime, endTime, time_spent;
    double parCurrentArray[dimension][dimension];
    double parNextArray[dimension][dimension];

    // rank 0 = master process
    if (rank == 0) {
        double seqCurrentArray[dimension][dimension];
        double seqNextArray[dimension][dimension];
        FILE * fp1 = fopen("/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/numbers.txt", "r+");
        // change array to be 2x2
        setArray(dimension, parCurrentArray, parNextArray, fp1);
        fclose(fp1);

        // only run sequential check from master thread
        sprintf(seqFilename, "/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/seqOut_%d.txt", dimension);
        if (numProcesses == 1) {
            FILE * fpRead = fopen("/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/numbers.txt", "r+");
            setArray(dimension, seqCurrentArray, seqNextArray, fpRead);
            fclose(fpRead);

            runSequential(dimension, seqCurrentArray, seqNextArray, precision, seqFilename);
        }
        // else send processes rows
        else {
            int row_start = 0;
            int row_end = 0;
            int i, j;
            MPI_Request sendReq[2];
            for (i = 0; i < numProcesses; i ++) {
                setThreadArraySections(&row_start, &row_end, dimension, numProcesses, i);
                for(j = row_start - 1; j < row_end + 1; j++) {
                    MPI_Isend(parCurrentArray[j], dimension, MPI_DOUBLE, i, row_start, MPI_COMM_WORLD, &sendReq[0]);
                    MPI_Isend(parNextArray[j], dimension, MPI_DOUBLE, i, row_start, MPI_COMM_WORLD, &sendReq[1]);
                }
            }
        }
    }

    int start_i = 0;
    int end_i = 0;
    setThreadArraySections(&start_i, &end_i, dimension, numProcesses, rank);
    int rows = end_i - start_i;
    int arraySize = rows+2;
    printf("rec rank: %d, start: %d, end: %d, arraySize: %d\n", rank, start_i, end_i, arraySize);
    double currentArray[arraySize][dimension];
    double nextArray[arraySize][dimension];

    // receive rows
    int j;
    MPI_Request recvReq[2];
    MPI_Status recvStat[2];
    for(j = 0; j < arraySize; j++) {
        MPI_Irecv(currentArray[j], dimension, MPI_DOUBLE, 0, start_i, MPI_COMM_WORLD, &recvReq[0]);
        MPI_Irecv(nextArray[j], dimension, MPI_DOUBLE, 0, start_i, MPI_COMM_WORLD, &recvReq[1]);
        MPI_Wait(&recvReq[0], &recvStat[0]);
        MPI_Wait(&recvReq[1], &recvStat[1]);
    }

    // barrier
    MPI_Barrier(MPI_COMM_WORLD);

    // Start parallel
    if (rank == 0) {
        beginTime = MPI_Wtime();
    }

    if (numProcesses != 1) {
        mpiParallel(dimension, start_i, end_i, arraySize, currentArray, nextArray, precision, rank, numProcesses);
    }
    printf("end parallel\n\n");


    // end parallel
    // send back all rows
    if (rank == 0) {
        endTime = MPI_Wtime();
    }

    MPI_Request sendReq[2];
    for(j = 0; j < arraySize; j++) {
        MPI_Isend(currentArray[j], dimension, MPI_DOUBLE, 0, start_i, MPI_COMM_WORLD, &sendReq[0]);
        MPI_Isend(nextArray[j], dimension, MPI_DOUBLE, 0, start_i, MPI_COMM_WORLD, &sendReq[1]);
    }

    if (rank == 0) {
        time_spent = endTime - beginTime;
        int row_start = 0;
        int row_end = 0;
        int i, j;
        MPI_Request endRecvReq[2];
        MPI_Status endRecvStat[2];
        for (i = 0; i < numProcesses; i ++) {
            setThreadArraySections(&row_start, &row_end, dimension, numProcesses, i);
            for(j = row_start - 1; j < row_end + 1; j++) {
                MPI_Irecv(parCurrentArray[j], dimension, MPI_DOUBLE, i, row_start, MPI_COMM_WORLD, &endRecvReq[0]);
                MPI_Irecv(parNextArray[j], dimension, MPI_DOUBLE, i, row_start, MPI_COMM_WORLD, &endRecvReq[1]);
                MPI_Wait(&endRecvReq[0], &endRecvStat[0]);
                MPI_Wait(&endRecvReq[1], &endRecvStat[1]);
            }
        }

        // read sequential result from file
        double seqArray1[dimension][dimension];
        double seqArray2[dimension][dimension];
        char firstLine[64];
        char precise[40];
        FILE * fpReadSeqArray = fopen(seqFilename, "r+");
        fgets(firstLine, 64, fpReadSeqArray);
        fgets(precise, 64, fpReadSeqArray);
        setArray(dimension, seqArray1, seqArray2, fpReadSeqArray);
        fclose(fpReadSeqArray);

        // Precision and Correctness tests
        printf("Array size %d by %d\n", dimension, dimension);
        printf("Sequential run\n");
        printf("%s", firstLine);
        printf("%s", precise);
        printf("Parallel run\n");
        printf("Number of Threads: %d\n", numProcesses);
        printf("Time: %lf seconds.\n", time_spent);
        int count = precisionTest(dimension, seqArray1, parCurrentArray, precision);
        if (count != 0) {
            printf("Not precise in %d places\n", count);
        } else {
            printf("Precise\n");
        }
        correctnessTest(dimension, seqArray1, parCurrentArray);
    }

    MPI_Finalize();
    return 0;
}
