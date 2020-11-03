
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

char *textData;
int textLength;

char *patternData;
int patternLength;

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

int readText ()
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
	readFromFile (f, &textData, &textLength);
	fclose (f);

	return 1;

}

// read pattern in a separate function since we have multiple patterns but only 1 text
int readPattern(int patternNumber)
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
    readFromFile (f, &patternData, &patternLength);
    fclose (f);

    printf ("Read pattern number %d\n", patternNumber);
}

int hostMatch(long *comparisons)
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

void processData()
{
	unsigned int result;
    long comparisons;

    result = hostMatch(&comparisons);
    if (result == -1)
        printf ("Pattern not found\n");
    else
        printf ("Pattern found at position %d\n", result);
    printf ("# comparisons = %ld\n", comparisons);

}

long getNanos(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

void runTest(int pid, int patternNumber)
{

    // time the entire program
    long timeStart, timeEnd;
    long elapsedNsec;
    timeStart = getNanos();

    processData();

    timeEnd = getNanos();

    elapsedNsec = (timeEnd - timeStart);
    printf("\nPID %i: Search pattern %i elapsed wall clock time = %ld\n", pid, patternNumber, (long)(elapsedNsec / 1.0e9));
    printf("PID %i: Search pattern %i elapsed CPU time = %.09f\n\n", pid, patternNumber, (double)elapsedNsec / 1.0e9);

}


int main(int argc, char **argv)
{
    /*
     * nProc - number of processes
     * procId - process Id
     * time - elapsed program time for a process
     * maxtime - longest program time reduced from process times
     */
    int nProc, procId;
    long time, maxtime;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nProc);
    MPI_Comm_rank(MPI_COMM_WORLD, &procId);

    readText();

    // throw error if less than 2 processes
    if (nProc < 2)
    {
        fprintf(stderr,"\nProgram requires 2 processes to run.\n");
        MPI_Finalize();
        exit(0);
    }

    // use master process to print out total processes and text length
    if (procId == 0)
    {
        printf("\n\nPID %i: Using %i processes\n\n", procId, nProc);
        printf ("Text length = %d\n\n", textLength);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    time = getNanos();


    int testNumber = procId+1;

    while (readPattern(testNumber))
    {
        runTest(procId, testNumber);
        testNumber += nProc;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    time = getNanos() - time;

    // call reduce to get the maximum elapsed time out of all processes
    // then take this value to be elapsed program time
    MPI_Reduce(&time, &maxtime, 1, MPI_LONG, MPI_MAX, 0, MPI_COMM_WORLD);

    // use master to get longest process time
    if (procId == 0)
    {
        printf("\nElapsed Program time using %i processes = %.09f\n\n", nProc, (double)maxtime / 1.0e9);
    }

    MPI_Finalize();


}