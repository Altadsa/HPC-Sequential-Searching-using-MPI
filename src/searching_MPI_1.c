
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>

#define MASTER 0

////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////


void outOfMemory()
{
	fprintf (stderr, "Out of memory\n");
	exit (0);
}

void readFromFile (FILE *f, char **data, int *length)
{
	int ch;
	int allocatedLength;
	char *result;
	int resultLength = 0;

	allocatedLength = 0;
	result = NULL;



	ch = fgetc (f);
	while (ch >= 0)
	{
		resultLength++;
		if (resultLength > allocatedLength)
		{
			allocatedLength += 10000;
			result = (char *) realloc (result, sizeof(char)*allocatedLength);
			if (result == NULL)
				outOfMemory();
		}
		result[resultLength-1] = ch;
		ch = fgetc(f);
	}
	*data = result;
	*length = resultLength;
}

int readText (char **textData, int *textLength)
{
	FILE *f;
	char fileName[1000];
#ifdef DOS
        sprintf (fileName, "inputs\\text.txt");
#else
	sprintf (fileName, "inputs/text.txt");
#endif
	f = fopen (fileName, "r");
	if (f == NULL)
		return 0;
	readFromFile (f, textData, textLength);
	fclose (f);

	return 1;

}

// read pattern in a separate function since we have multiple patterns but only 1 text
int readPattern(int patternNumber, char **patternData, int *patternLength)
{
    FILE *f;
    char fileName[1000];
#ifdef DOS
    sprintf (fileName, "inputs\\pattern%i.txt", patternNumber);
#else
    sprintf (fileName, "inputs/pattern%i.txt", patternNumber);
#endif
    f = fopen (fileName, "r");
    if (f == NULL)
        return 0;
    readFromFile (f, patternData, patternLength);
    fclose (f);

    printf ("Read pattern number %d\n", patternNumber);
}

int hostMatch(long *comparisons, char *textData, int textLength, char *patternData, int patternLength)
{
	int i,j,k, lastI;

	i=0;
	j=0;
	k=0;
	lastI = textLength-patternLength;
        *comparisons=0;

	while (i<=lastI && j<patternLength)
	{
                (*comparisons)++;
		if (textData[k] == patternData[j])
		{
			k++;
			j++;
		}
		else
		{
			i++;
			k=i;
			j=0;
		}
	}

	if (j == patternLength)
	{
        return i;
    }
	else
		return -1;
}

// calculate current time in nanoseconds
long getNanos(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

/// <summary>
/// Searches for a pattern within a section of text.
/// </summary>
/// <param name="procId">Process ID.</param>
/// <param name="startIndex">The displacement within the full text.</param>
/// <param name="textData">The partial text data received from the master.</param>
/// <param name="textLength">The length of the allocated text data.</param>
/// <param name="patternData">The pattern data received from the master.</param>
/// <param name="patternLength">The length of the pattern data.</param>
/// <returns></returns>
int processData(int procId, int startIndex, char* textData, int textLength, char* patternData, int patternLength)
{
    int result;
    long comparisons;
    result = hostMatch(&comparisons, textData, textLength, patternData, patternLength);

    //printf("PID %i: # comparisons = %ld\n", procId, comparisons);
    if (result == -1)
    {
        printf("PID %i: Pattern not found, return %i\n", procId, result);
        return result;
    }
    else
    {

        printf("PID %i: Pattern found at position %d, global position %i\n", procId, result, (result+startIndex));

        // we add the start index to get the actual position in the whole text
        result += startIndex;
        
        return result;
    }
}

/// <summary>
/// Distributes text length among processes.
/// </summary>
/// <param name="procWork">Array to contain the workload (length of allocated text) of each process.</param>
/// <param name="nProc">Number of processes in the program.</param>
/// <param name="textLength">The length of the full text.</param>
void divideWorkload(int *procWork, int nProc, int textLength)
{
    // calculate the base number of elements for each process
    int nElements = textLength / nProc;
    //printf("\nnElements = %i\n", nElements); // confirm base elements 

    // set base number of elements for each process i
    int i;
    for (i = 0; i < nProc; i++)
    {
        (*(procWork + i)) = nElements;
    }

    int remainder = textLength % nProc;
    //printf("\nRemainder = %i\n", remainder); // confirm remainder

    // if there are no remainders, we can continue with the program
    if (remainder == 0)
    {
        return;
    }

    // assign remainders to slave processes
    // we work backwards through processes such that only slave processes
    // take on any extra workload
    for (i = (nProc-1); i > ((nProc-1)-remainder); i--)
    {
        (*(procWork + i)) += 1;
    }
}

/// <summary>
/// Sets the displacement in the full text for each process.
/// </summary>
/// <param name="displs">Array to contain the displacement of each process.</param>
/// <param name="procWork">Array containing the workload of each process.</param>
/// <param name="nProc">Number of processes in the program.</param>
void setDisplacement(int* displs, int* procWork, int nProc)
{
    int i;
    // displacement at i dependent on i-1. We can set displs[0] to 0 since we know it starts there
    displs[0] = 0;
    for (i = 1; i < nProc; i++)
    {
        displs[i] = displs[i - 1] + procWork[i - 1];
        //printf("Process %i displacement = %i\n", i, displs[i]);
    }
}

/// <summary>
/// Prints out the number of elements to be received by each process. 
/// For Debugging purposes only.
/// </summary>
/// <param name="procWorkload">Pointer to array containing the workloads for each elements.</param>
/// <param name="nProc">The number of processes used in the program.</param>
void debugPrintWorkload(int *procWorkload, int nProc)
{
    int i;
    for (i = 0; i < nProc; i++)
    {
        printf("\nProcess %i receiving %i elements\n", i, (*(procWorkload + i)));
    }
}

/// <summary>
/// Carries out master section of program, such as reading text and assignment of text sub-array, division of elements
/// and calculation of final result (by reduction).
/// </summary>
/// <param name="result">The result to be assigned by hostMatch, and reduced upon completion of all process searches.</param>
/// <param name="nProc">The number of processes for this program.</param>
/// <param name="textData">Pointer to array of text data</param>
/// <param name="textLength"></param>
/// <param name="patternData"></param>
/// <param name="patternLength"></param>
void processMaster(int result, int nProc)
{
    // delcare data variables
    char* textData;
    int textLength;

    char* patternData;
    int patternLength;

    // should load the text data for this process only
    readText(&textData, &textLength);

    // store number of elements each process receives
    int* displs = (int*)malloc(nProc * sizeof(int));
    int* procWorkload = (int*)malloc(nProc * sizeof(int));

    //printf("\nDividing text of length %i among %i processes.\n", textLength, nProc);

    // divide the workload among the processes
    divideWorkload(procWorkload, nProc, textLength);
    //debugPrintWorkload(procWorkload, nProc);

    // get the displacement within the text for each process
    setDisplacement(displs, procWorkload, nProc);

    int i, nElements; // iterable, search elements per process
    int patternNumber = 1;
    int startIndex = 0; // actual start index of text for process

    // divide text and send to slave processes
    // since there is only 1 text for this assignment, this data only needs
    // to be sent once

    // scatter workload to processes so they know how many elements are being received
    MPI_Scatter(procWorkload, 1,
        MPI_INT, &nElements, 1, 
        MPI_INT, MASTER,
        MPI_COMM_WORLD);

    // scatter the displacement to get the actual text index and not the relative index
    MPI_Scatter(displs, 1,
        MPI_INT, &startIndex, 1,
        MPI_INT, MASTER,
        MPI_COMM_WORLD);

    // scatter the text data to each process
    // we use scatterv under the assumption that the workload MIGHT not be evenly distributed
    // even though the text data can be evenly distributed
    MPI_Scatterv(textData,
        procWorkload, displs,
        MPI_CHAR, textData, 1, 
        MPI_CHAR, MASTER,
        MPI_COMM_WORLD);

    // flag used to tell slave processes to stop 
    int finished = 0;

    // Used to time program
    long searchTime;

    // stores reduced result value
    int finalResult;
    while (readPattern(patternNumber, &patternData, &patternLength))
    {

        searchTime = getNanos();

        //printf("\nPattern %i of Length = %i\n", patternNumber,patternLength); // confirm pattern length

        // broadcast the finished flag before pattern data
        // this ensures we avoid deadlock when we are actually finished
        // since it is received before pattern data

        MPI_Bcast(&finished, 
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // we can broadcast pattern data since each process is receiving the same values

        // send pattern length first so slaves know
        // how large the received pattern is
        MPI_Bcast(&patternLength,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // send entire pattern to slaves
        MPI_Bcast(patternData,
            patternLength, MPI_CHAR, MASTER,
            MPI_COMM_WORLD);

        // pattern search for the master process
        result = processData(0, 0, textData, nElements, patternData, patternLength);

        // get final result by reducing result into the master with a max operation
        // this assumes at most one pattern occurrence in the text

        MPI_Reduce(&result, &finalResult, 
            1, MPI_INT, MPI_MAX, MASTER,
            MPI_COMM_WORLD);

        searchTime = getNanos() - searchTime;

        // print final result
        printf("\nPattern %i: Pattern found at position %i\n\n", patternNumber, finalResult);

        // print elapsed search time
        printf("\nPattern %i elapsed wall clock time = %ld\n", patternNumber, (long)(searchTime / 1.0e9));
        printf("Pattern %i search elapsed CPU time = %.09f\n\n", patternNumber, (double)searchTime / 1.0e9);


        // free allocated pattern memory
        free(patternData);

        // move to next pattern (if possible)
        patternNumber++;
    }

    // set finished to true and broadcast to slaves
    // this way they receive the finsh flag and exit their loop
    // before trying to receive the pattern data

    finished = 1;
    MPI_Bcast(&finished,
        1, MPI_INT, MASTER,
        MPI_COMM_WORLD);

    // free text memory
    free(textData);
}

void processSlave(int procId, int result)
{

    // delcare data variables
    char* textData;
    int textLength;

    char* patternData;
    int patternLength;

    // receive the text length before the data
    MPI_Scatter(NULL, 1,
        MPI_INT, &textLength, 1,
        MPI_INT, MASTER,
        MPI_COMM_WORLD);

    // receive displacement in text data to calculate actual result
    int startIndex;
    MPI_Scatter(NULL, 1,
        MPI_INT, &startIndex, 1,
        MPI_INT, MASTER,
        MPI_COMM_WORLD);

    // allocate text data based on number of received elements
    textData = (char*)malloc(textLength * sizeof(char));
    // receive text data from master
    MPI_Scatterv(NULL,
        NULL, NULL,
        MPI_CHAR, textData, textLength,
        MPI_CHAR, MASTER,
        MPI_COMM_WORLD);

    // finished flag to be received from master
    int finished = 0;

    // confirm data received ok
    printf("\nPID %i received text data, searching from index %i for %i elements.\n", procId, startIndex, textLength);

    // loop until told to finish by master
    while (!finished)
    {

        // receive finish flag before trying to receive pattern data
        MPI_Bcast(&finished,
            1, MPI_INT, 0,
            MPI_COMM_WORLD);

        // if master tells slave to finish, return from function and do not try to receive pattern data
        if (finished)
        {
            return;
        }

        // receive pattern length before pattern data
        MPI_Bcast(&patternLength,
            1, MPI_INT, MASTER,
            MPI_COMM_WORLD);

        // allocate memory for pattern data
        patternData = (char*)malloc(patternLength * sizeof(char));

        // receive pattern data after getting the number of received elements
        MPI_Bcast(patternData,
            patternLength, MPI_CHAR, MASTER,
            MPI_COMM_WORLD);

        // pattern search for slave process
        result = processData(procId, startIndex, textData, textLength, patternData, patternLength);

        // reduce the result to the master process
        MPI_Reduce(&result, NULL,
            1, MPI_INT, MPI_MAX, MASTER,
            MPI_COMM_WORLD);

        // free allocated pattern memory
        free(patternData);

    }

    // free allocated text memory
    free(textData);

}

int main(int argc, char **argv)
{
    
    // number of processes and process id
    int nProc, procId;

    // result of the search
    unsigned int result;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nProc);
    MPI_Comm_rank(MPI_COMM_WORLD, &procId);

    // throw error if less than 2 processes
    if (nProc < 2)
    {
        fprintf(stderr,"\nProgram requires 2 processes to run.\n");
        MPI_Finalize();
        exit(0);
    }

    if (procId == MASTER) // master process
    {
        processMaster(result, nProc);
    }
    else // slave processes
    {
        processSlave(procId, result);
    }

    MPI_Finalize();


}