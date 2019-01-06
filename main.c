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

/* Function that sets the values of the start and end index of the array that each process has to work on.
 * Does the calculation to split the array and the remainder rows.
 * start_i and end_i are pointers to the integers that hold the start and end row indices that a given
 * process (id is the process id)
 */
void setThreadArraySections(int *start_i, int *end_i, int dimension, int numProcesses, int id){
    int remainder, rowsToUse, numRows;

    // numRows works out which rows are to be written to by removing 2 from the dimension (the top and bottom borders)
    numRows = dimension - 2;
    // rowsToUse splits the number of rows (numRows) by the number of processes (numProcesses)
    rowsToUse = (int) floor(numRows / numProcesses);
    // check if there is a remainder
    // split the remainder and use to set the start index
    remainder = (numRows % numProcesses);
    if (remainder - id > 0) {
        *start_i = id * rowsToUse + id + 1;
        rowsToUse++;
    }
    else {
        *start_i = id * rowsToUse + remainder + 1;
    }
    // set end index
    *end_i = *start_i + rowsToUse;
}

/* Runs the sequential algorithm
 * The current and next arrays are pointer-to-pointer data structures to represent the 2D array
 * The dimension is used to loop over the array and the precision is used to check if the right precision
 * is met.
 */
void sequentialSolver(int dimension, double **currentArray, double **nextArray, double precision) {
    double a, b, c, d, av, current, diff;
    int i, j;
    // stop = 0 means that the loop should continue and stop = 1 means that the loop should stop
    int stop = 0;
    // used to swap local array pointers after each iteration
    int count = 0;
    double **readArray = currentArray;
    double **writeArray = nextArray;

    // loop exits when stop is set to 1
    while (stop == 0) {
        // start by setting it to make the loop stop
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
                // if the loop was set to stop and the difference is still bigger than the precision
                // loop needs to continue and this is done by setting stop variable to 0
                if (stop == 1 & diff > precision) {
                    stop = 0;
                }
            }
        }

        // Swap local array pointers to read from the array that has been updated
        // even iterations will set local pointers like so and set count variable to be 1 (odd)
        if (count == 0) {
            readArray = nextArray;
            writeArray = currentArray;
            count++;
        }
        // odd iterations will set local pointers like so (swaps back) and set count variable back to 0 (even)
        else {
            readArray = currentArray;
            writeArray = nextArray;
            count--;
        }
    }
}

/* This is the parallel version of the algorithm that is run by each process
 * The current and next arrays are pointer-to-pointer data structures to represent the 2D array
 * The dimension and the arraySize to loop over the array sections for each process (arraySize includes
 * the above and below border rows that are used for reading)
 * Rows represents the number of rows that the process needs to write to (excludes border rows)
 * Precision is used to check if the right precision is met.
 * Rank is the process rank and numProcesses is the number of processes in the communicator.
 */
void parallelSolver(int dimension, int rows, int arraySize, double **currentArray, double **nextArray, double precision, int rank, int numProcesses) {
    double a, b, c, d, av, current, diff;
    int i, j;
    // stop = 0 means that the loop should continue and stop = 1 means that the loop should stop
    int stop = 0;

    // used to swap local array pointers after each iteration
    int count = 0;
    double **readArray = currentArray;
    double **writeArray = nextArray;

    // loop exits when break is called
    while (1) {
        // start by setting it to make the loop stop
        stop = 1;
        // iterates over process specific array section
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
                // if the loop was set to stop and the difference is still bigger than the precision
                // loop needs to continue and this is done by setting stop variable to 0
                if (stop == 1 & diff > precision) {
                    stop = 0;
                }
            }
        }

        // barrier to wait for all processes to complete calculations before checking stop and swapping arrays
        MPI_Barrier(MPI_COMM_WORLD);

        // broadcast stop to other processes
        int process;

        // stop is local whereas broadcastStop is written to by the MPI Broadcast
        int broadcastStop = 0;
        for (process = 0; process<numProcesses; process++){
            // if self then set broadcastStop to the value of the local stop variable
            if (rank == process) {
                broadcastStop = stop;
            }
            // get broadcast value of broadcastStop from all processes
            MPI_Bcast(&broadcastStop, 1, MPI_INT, process, MPI_COMM_WORLD);
            // break for loop if any process needs to continue calculation
            if (broadcastStop == 0) {
                break;
            }
        }

        // check broadcastStop to exit while loop
        if (broadcastStop == 1) {
            break;
        }

        // Send written rows to processes that read from those rows
        MPI_Request req[4];
        MPI_Status recvStat[2];

        // send above neighbour (rank - 1)
        // check for top process
        if (rank - 1 >= 0){
            MPI_Isend(writeArray[1], dimension, MPI_DOUBLE, rank - 1, 98, MPI_COMM_WORLD, &req[0]);
        }

        // send below neighbour (rank + 1)
        // check for end process
        if (rank + 1 < numProcesses) {
            MPI_Isend(writeArray[rows], dimension, MPI_DOUBLE, rank + 1, 99, MPI_COMM_WORLD, &req[1]);
        }

        // Receive written rows from processes that write to those rows
        // receive above neighbour (rank - 1)
        // check for top process
        if (rank - 1 >= 0){
            MPI_Irecv(writeArray[0], dimension, MPI_DOUBLE, rank - 1, 99, MPI_COMM_WORLD, &req[2]);
        }


        // receive below neighbour (rank + 1)
        // check for end process
        if (rank + 1 < numProcesses) {
            MPI_Irecv(writeArray[rows+1], dimension, MPI_DOUBLE, rank + 1, 98, MPI_COMM_WORLD, &req[3]);
        }

        // Wait for receive to complete before continuing
        if (rank - 1 >= 0) {
            MPI_Wait(&req[2], &recvStat[0]);
        }

        if (rank + 1 < numProcesses) {
            MPI_Wait(&req[3], &recvStat[1]);
        }

        // Swap local array pointers to read from the array that has been updated
        // even iterations will set local pointers like so and set count variable to be 1 (odd)
        if (count == 0) {
            readArray = nextArray;
            writeArray = currentArray;
            count++;
        }
        // odd iterations will set local pointers like so (swaps back) and set count variable back to 0 (even)
        else {
            readArray = currentArray;
            writeArray = nextArray;
            count--;
        }
    }
}

/* Checks if the difference between the final two arrays is less than the precision
 * This test is to check if the loop has exited too early for either algorithm
 * If there are cells that are imprecise, the count variable is incremented
 * This count variable is returned and checked when precision test is called
 */
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

/* Checks if there is a difference between the sequential result array and the parallel result array
 * If the difference between a given cell in the sequential array and a given cell in the parallel array
 * is greater than a negligible amount then the difference is put into the result array and the count is
 * incremented.
 * If the count is 0 the parallel is correct (matches the sequential array)
 * If not then the count is printed and the results array is printed to see which cells where incorrect
 */
void correctnessTest(int dimension, double **seqArray, double **parArray) {
    int k;
    // Declare results array and allocate memory to it
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
            // 0,000001 is negligible enough
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

/* Runs the sequential algorithm for a given array and outputs the following to a file with a given filename:
 *  - the time taken
 *  - the result of the precision check
 *  - the final array (both are the same for the sequential algorithm
 *
 */
void runSequential(int dimension, double **currentArray, double **nextArray, double precision, char * filename) {
    double begin, end;
    unsigned long long int time_spent;
    // using the MPI_Wtime function that returns the seconds passed as a double
    begin = MPI_Wtime();
    sequentialSolver(dimension, currentArray, nextArray, precision);
    end = MPI_Wtime();

    // get the total time spent as a unsigned long long integer and convert it to nanoseconds
    time_spent = (unsigned long long int)((end - begin) * 1000000000L);

    // write out results to file
    FILE * fpWrite = fopen(filename, "w+");
    fprintf(fpWrite, "Time: %llu nanoseconds.\n", time_spent);
    // call precision test and print results to file
    int count = precisionTest(dimension, currentArray, nextArray, precision);
    if(count != 0){
        fprintf(fpWrite, "Not precise in %d places\n", count);
    }
    else {
        fprintf(fpWrite, "Precise\n");
    }
    int i, j;
    // print final currentArray to file
    for (i = 0; i < dimension; i++) {
        for (j = 0; j < dimension; j++) {
            fprintf(fpWrite, "%lf,", currentArray[i][j]);
        }
    }
    fclose(fpWrite);
}

/* Main function is run by each process
 * Has 2 parameters:
 *  - array dimension
 *  - precision
 *  MPI communicator gives the number of processes that this is run with and the rank (id) of the current process
 *  Gets the array section for a given process and hence the number of rows it needs to write to and the arraySize
 *  it will need (includes 2 border rows to read from)
 *  Fills an array of size dimension squared with values read in from the file of comma separated
 *  numbers
 *  Master process runs sequential with that array if number of processes is 1
 *  Runs parallel if number of processes are greater than 1 and master array checks the correctness by reading in the
 *  sequential result for that array dimension
 */
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

    // Pointer-to-pointer data structures used to represent the 2D arrays
    // 2 for full array and 2 for current processes array sections
    double **parCurrentArray;
    double **parNextArray;
    double **currentArray;
    double **nextArray;
    // if main process allocate memory for the whole array (dimension x dimension)
    // else allocate memory according to current process' arraySize (arraySize x dimension)
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
        // Run sequential from master process
        // Get output filename for sequential run based on run parameters
        sprintf(seqFilename, "/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/seqOut_%d.txt", dimension);
        if (numProcesses == 1) {
            // Declare sequential arrays and allocate memory to them
            double **seqCurrentArray;
            double **seqNextArray;
            seqCurrentArray = (double**)malloc(sizeof(double*) * dimension);
            seqNextArray = (double**)malloc(sizeof(double*) * dimension);
            for(j = 0; j < dimension; j++) {
                seqCurrentArray[j] = (double*)malloc(sizeof(double) * dimension);
                seqNextArray[j] = (double*)malloc(sizeof(double) * dimension);
            }
            // Read numbers into sequential arrays from numbers file (depends on dimension)
            FILE * fpRead = fopen("/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/numbers.txt", "r+");
            char pwd[100];
            setArray(dimension, seqCurrentArray, seqNextArray, fpRead);
            fclose(fpRead);

            // Run Sequential
            runSequential(dimension, seqCurrentArray, seqNextArray, precision, seqFilename);
        }
        // if not sequential run send other processes the right rows to read and write to
        else {
            // Read numbers into whole parallel arrays from numbers file (depends on dimension)
            FILE * fp1 = fopen("/home/janhavi/CLionProjects/parallel_cw2/cmake-build-mpi/numbers.txt", "r+");
            setArray(dimension, parCurrentArray, parNextArray, fp1);
            fclose(fp1);

            // Gets array sections for each other process then sends them rows accordingly
            MPI_Request sendReq[2];
            int processRowStart, processRowEnd;
            for (i = 1; i < numProcesses; i ++) {
                setThreadArraySections(&processRowStart, &processRowEnd, dimension, numProcesses, i);
                for(j = processRowStart - 1; j < processRowEnd + 1; j++) {
                    MPI_Isend(parCurrentArray[j], dimension, MPI_DOUBLE, i, processRowStart, MPI_COMM_WORLD, &sendReq[0]);
                    MPI_Isend(parNextArray[j], dimension, MPI_DOUBLE, i, processRowStart, MPI_COMM_WORLD, &sendReq[1]);
                }
            }
            // Don't use mpi send for rank 0
            for(j = row_start - 1; j < row_end + 1; j++) {
                currentArray[j] = parCurrentArray[j];
                nextArray[j] = parNextArray[j];
            }
        }
    }
    // If not master process , need to receive current process' array section rows
    else {
        // Wait for each receive row
        MPI_Request recvReq[2];
        MPI_Status recvStat[2];
        for(j = 0; j < arraySize; j++) {
            MPI_Irecv(currentArray[j], dimension, MPI_DOUBLE, 0, row_start, MPI_COMM_WORLD, &recvReq[0]);
            MPI_Irecv(nextArray[j], dimension, MPI_DOUBLE, 0, row_start, MPI_COMM_WORLD, &recvReq[1]);
            MPI_Wait(&recvReq[0], &recvStat[0]);
            MPI_Wait(&recvReq[1], &recvStat[1]);
        }
    }

    // barrier before time starts for measuring time taken for parallel algorithm
    MPI_Barrier(MPI_COMM_WORLD);

    // Start parallel time
    if (rank == 0) {
        beginTime = MPI_Wtime();
    }

    // if not sequential run parallel solver for current process
    if (numProcesses != 1) {
        parallelSolver(dimension, rows, arraySize, currentArray, nextArray, precision, rank, numProcesses);
    }

    // End parallel time
    if (rank == 0) {
        endTime = MPI_Wtime();
    }

    // if not sequential run
    if (numProcesses > 1) {
        // send back all rows if current process is not master process
        if (rank > 0) {
            MPI_Request sendReq[2];
            for (j = 0; j < arraySize; j++) {
                MPI_Isend(currentArray[j], dimension, MPI_DOUBLE, 0, row_start, MPI_COMM_WORLD, &sendReq[0]);
                MPI_Isend(nextArray[j], dimension, MPI_DOUBLE, 0, row_start, MPI_COMM_WORLD, &sendReq[1]);
            }
        }
            // if current process is master process
        else {
            // calculate parallel time spent
            time_spent = (long) ((endTime - beginTime) * 1000000000L);

            // receive all rows from other processes (not including rank 0
            MPI_Request endRecvReq[2];
            MPI_Status endRecvStat[2];
            for (i = 1; i < numProcesses; i++) {
                setThreadArraySections(&row_start, &row_end, dimension, numProcesses, i);
                for (j = row_start - 1; j < row_end + 1; j++) {
                    MPI_Irecv(parCurrentArray[j], dimension, MPI_DOUBLE, i, row_start, MPI_COMM_WORLD, &endRecvReq[0]);
                    MPI_Irecv(parNextArray[j], dimension, MPI_DOUBLE, i, row_start, MPI_COMM_WORLD, &endRecvReq[1]);
                    MPI_Wait(&endRecvReq[0], &endRecvStat[0]);
                    MPI_Wait(&endRecvReq[1], &endRecvStat[1]);
                }
            }

            // Declare read-in sequential arrays and allocate memory to them (if not sequential run)
            double **readSeqCurrentArray;
            double **readSeqNextArray;
            readSeqCurrentArray = (double **) malloc(sizeof(double *) * dimension);
            readSeqNextArray = (double **) malloc(sizeof(double *) * dimension);
            for (j = 0; j < dimension; j++) {
                readSeqCurrentArray[j] = (double *) malloc(sizeof(double) * dimension);
                readSeqNextArray[j] = (double *) malloc(sizeof(double) * dimension);
            }

            // read sequential result from file
            char seqTimeSpent[64];
            char precise[40];
            FILE *fpReadSeqArray = fopen(seqFilename, "r+");
            fgets(seqTimeSpent, 64, fpReadSeqArray);
            fgets(precise, 64, fpReadSeqArray);
            setArray(dimension, readSeqCurrentArray, readSeqNextArray, fpReadSeqArray);
            fclose(fpReadSeqArray);

            // Print out sequential results for precision and time spent
            printf("Array size %d by %d\n", dimension, dimension);
            printf("Sequential run\n");
            printf("%s", seqTimeSpent);
            printf("%s", precise);
            // Print parallel time spent for given number of processes
            printf("Parallel run\n");
            printf("Number of Processes: %d\n", numProcesses);
            printf("Time: %llu nanoseconds.\n", (unsigned long long int) time_spent);
            // Precision and Correctness tests
            int count = precisionTest(dimension, readSeqCurrentArray, parCurrentArray, precision);
            if (count != 0) {
                printf("Not precise in %d places\n", count);
            } else {
                printf("Precise\n");
            }
            correctnessTest(dimension, readSeqCurrentArray, parCurrentArray);
        }
    }

    MPI_Finalize();
    return 0;
}
