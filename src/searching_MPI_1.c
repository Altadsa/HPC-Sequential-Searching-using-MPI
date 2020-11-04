
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>

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
        printf("PID %i: Pattern found at position %d\n", procId, result);

        // we add the start index to get the actual position in the whole text
        result += startIndex;
        
        return result;
    }
    

    

}

long getNanos(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

void divideWorkload(int *procWork, int nProc, int textLength)
{
    int nElements = textLength / nProc;
    //printf("\nnElements = %i\n", nElements);
    int i;
    for (i = 0; i < nProc; i++)
    {
        (*(procWork + i)) = nElements;
    }

    int remainder = textLength % nProc;
    //printf("\nRemainder = %i\n", remainder);

    // assign remainders to slave processes
    // hence why we work backwards through processes
    if (remainder == 0)
    {
        return;
    }

    for (i = (nProc-1); i > ((nProc-1)-remainder); i--)
    {
        (*(procWork + i)) += 1;
    }

}

void debugPrintWorkload(int *procWorkload, int nProc)
{
    int i;
    for (i = 0; i < nProc; i++)
    {
        printf("\nProcess %i receiving %i elements\n", i, (*(procWorkload + i)));
    }
}

char* textData;
int textLength;

char* patternData;
int patternLength;

void processMaster(int result, int nProc, char *textData, int textLength, char *patternData, int patternLength)
{
    // should load the text data for this process only
    readText(&textData, &textLength);

    // store number of elements each process receives
    int* procWorkload = (int*)malloc(nProc * sizeof(int));
    int* results;

    // print to confirm
    //printf("\nDividing text of length %i among %i processes.\n", textLength, nProc);

    // divide the workload among the processes
    divideWorkload(procWorkload, nProc, textLength);
    //debugPrintWorkload(procWorkload, nProc);

    int i, nElements, startIndex;
    int patternNumber = 1;

    // divide text and send to slave processes
    for (i = 1; i < nProc; i++)
    {
        // get number of elements to search

        nElements = (*(procWorkload + i));
        MPI_Send(&nElements,
            1, MPI_INT, i, 0,
            MPI_COMM_WORLD);

        startIndex = nElements * i;

        // send part of text data

        MPI_Send(&startIndex,
            1, MPI_INT, i, 0,
            MPI_COMM_WORLD);

        MPI_Send(&textData[startIndex],
            nElements, MPI_CHAR, i, 0,
            MPI_COMM_WORLD);
    }

    int finished = 0;
    int activeThreads;
    while (readPattern(patternNumber, &patternData, &patternLength))
    {
        printf("\nPattern Length = %i\n", patternLength);

        MPI_Bcast(&finished, 
            1, MPI_INT, 0, 
            MPI_COMM_WORLD);

        // divide text and send to slave processes
        for (i = 1; i < nProc; i++)
        {
            activeThreads++;

            // send pattern data

            MPI_Send(&patternLength,
                1, MPI_INT, i, 0,
                MPI_COMM_WORLD);

            MPI_Send(patternData,
                patternLength, MPI_CHAR, i, 0,
                MPI_COMM_WORLD);

        }

        // perform pattern search for this process
        nElements = (*(procWorkload + 0));
        result = processData(0, 0, textData, nElements, patternData, patternLength);

        //results = (int*)malloc((nProc-1)*sizeof(int));

        //int r;
        //while (activeThreads > 0)
        //{

        //    MPI_Recv(&r,
        //        1, MPI_UNSIGNED,
        //        MPI_ANY_SOURCE, 0,
        //        MPI_COMM_WORLD,
        //        MPI_STATUS_IGNORE);

        //    printf("Received value of %i\n", r);
        //    //results[(nProc - 1) - activeThreads] = r;
        //    if (r != 0)
        //        activeThreads--;

        //}

        int finalResult;
        MPI_Reduce(&result, &finalResult, 
            1, MPI_INT, MPI_MAX, 0, 
            MPI_COMM_WORLD);

        //results = (int*)malloc(nProc * sizeof(int));

        //MPI_Gather(&result, 1, MPI_INT, &results, 1, MPI_INT, 0, MPI_COMM_WORLD);
        //for (i = 0; i < nProc; i++)
        //{
        //    if (results[i] > result)
        //    {
        //        result = results[i];
        //    }
        //}

        printf("\nPattern %i: Pattern found at position %i\n\n", patternNumber, finalResult);

        patternNumber++;
        //free(results);
    }

    finished = 1;
    MPI_Bcast(&finished,
        1, MPI_INT, 0,
        MPI_COMM_WORLD);

}

void processSlave(int procId, int result, char* textData, int textLength, char* patternData, int patternLength)
{
    // receive the text data
    MPI_Recv(&textLength,
        1, MPI_INT, 0, 0,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE);

    int startIndex;
    MPI_Recv(&startIndex,
        1, MPI_INT, 0, 0,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE);

    textData = (char*)malloc(textLength * sizeof(char));
    MPI_Recv(textData,
        textLength, MPI_CHAR, 0, 0,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE);

    int finished = 0;

    printf("\nPID %i received text data, searching from index %i for %i elements.\n", procId, startIndex, textLength);

    while (!finished)
    {
        MPI_Bcast(&finished,
            1, MPI_INT, 0,
            MPI_COMM_WORLD);

        if (!finished)
        {
            MPI_Recv(&patternLength,
                1, MPI_INT, 0, 0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            patternData = (char*)malloc(patternLength * sizeof(char));
            MPI_Recv(patternData,
                patternLength, MPI_CHAR, 0, 0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            result = processData(procId, startIndex, textData, textLength, patternData, patternLength);

            MPI_Reduce(&result, NULL, 
                1, MPI_INT, MPI_MAX, 0,
                MPI_COMM_WORLD);

            //MPI_Send(&result,
            //    1, MPI_UNSIGNED, 0, 0,
            //    MPI_COMM_WORLD);
        }

    }



}

int main(int argc, char **argv)
{
    
    // number of processes and process id
    int nProc, procId;
    // elapsed program time
    long time, maxtime;

    unsigned int result;
    long comparisons;

    char* textData;
    int textLength;

    char* patternData;
    int patternLength;

    MPI_Status status;

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

    if (procId == 0) // master process
    {
        processMaster(result, nProc, textData, textLength, patternData, patternLength);
    }
    else // slave processes
    {
        processSlave(procId, result, textData, textLength, patternData, patternLength);
    }

    MPI_Finalize();


}