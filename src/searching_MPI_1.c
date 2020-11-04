
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

int readText (/*char *textData, int textLength*/)
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


void divideWorkload(int *procWork, int nProc, int textLength)
{
    int nElements = textLength / nProc;
    printf("\nnElements = %i\n", nElements);
    int i;
    for (i = 0; i < nProc; i++)
    {
        (*(procWork + i)) = nElements;
    }

    int remainder = textLength % nProc;
    printf("\nRemainder = %i\n", remainder);

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

void processMaster()
{

}

void processSlave()
{

}

int main(int argc, char **argv)
{
    
    // number of processes and process id
    int nProc, procId;
    // elapsed program time
    long time, maxtime;

    unsigned int result;
    long comparisons;

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
        // should load the text data for this process only
        readText();

        // store number of elements each process receives
        int *procWorkload = (int*)malloc(nProc*sizeof(int));

        // print to confirm
        printf("\nDividing text of length %i among %i processes.\n", textLength, nProc);

        // divide the workload among the processes
        divideWorkload(procWorkload, nProc, textLength);
        debugPrintWorkload(procWorkload, nProc);

        

        printf("\nPID %i: Text Length = %i\n", procId, textLength);
        long programTime = getNanos();
        char* masterTextData;
        int nElements;
        int startIndex;
        int i;
        int patternNumber = 1;

        //readPattern(patternNumber);

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

        while (readPattern(patternNumber))
        {

            int activeThreads = 0;

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


            printf("PID %i: searching %i elements\n", procId, (*(procWorkload + procId)));
            textLength = (*(procWorkload + procId));
            //memcpy(textData, masterTextData, (*(procWorkload + i)) * sizeof(char));

            result = hostMatch(&comparisons);
            if (result == -1)
                printf("Pattern not found\n");
            else
                printf("PID %i: Pattern found at position %d\n", procId, result);
            printf("PID %i: # comparisons = %ld\n", procId, comparisons);

            printf("\nEnter while\n");
            int r;
            while (activeThreads > 0)
            {

                MPI_Recv(&r,
                    1, MPI_UNSIGNED,
                    MPI_ANY_SOURCE, 0,
                    MPI_COMM_WORLD,
                    &status);

                printf("Received value of %i\n", r);

                if (r != 0)
                    activeThreads--;

                //MPI_Recv(&comparisons,
                //    1, MPI_LONG,
                //    OMP_ANY_SOURCE, 0,
                //    MPI_COMM_WORLD,
                //    &status);

                

            }

            printf("\nExit while\n");
            programTime = getNanos() - programTime;

            //unsigned int finalResult;
            //MPI_Reduce(&result, &finalResult, 1, MPI_UNSIGNED, MPI_MAX, 0, MPI_COMM_WORLD);

            //long maxComparisons;
            //MPI_Reduce(&comparisons, &maxComparisons, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

            //if (result == -1)
            //    printf("\nPattern not found\n");
            //else
            //    printf("\nPattern found at position %d\n", result);


            //printf("\nPattern %i: # comparisons = %ld\n", patternNumber, comparisons);

            //patternNumber++;
        }


    }
    else // slave processes
    {

        MPI_Recv(&textLength,
            1, MPI_INT, 0, 0,
            MPI_COMM_WORLD,
            &status);

        int startIndex;
        MPI_Recv(&startIndex,
            1, MPI_INT, 0, 0,
            MPI_COMM_WORLD,
            &status);

        textData = (char*)malloc(textLength * sizeof(char));
        MPI_Recv(textData,
            textLength, MPI_CHAR, 0, 0,
            MPI_COMM_WORLD,
            &status);



        MPI_Recv(&patternLength,
            1, MPI_INT, 0, 0,
            MPI_COMM_WORLD,
            &status);

        patternData = (char*)malloc(patternLength * sizeof(char));
        MPI_Recv(patternData,
            patternLength, MPI_CHAR, 0, 0,
            MPI_COMM_WORLD,
            &status);

        printf("\nPID %i: Searching %i elements, Pattern Length = %i\n", procId, textLength, patternLength);

        

        result = hostMatch(&comparisons);
        if (result == -1)
            printf("Pattern not found\n");
        else
            printf("PID %i: Pattern found at position %d\n", procId, (startIndex+result));
        printf("PID %i: # comparisons = %ld\n", procId, comparisons);

        MPI_Send(&result,
            1, MPI_UNSIGNED, 0, 0,
            MPI_COMM_WORLD);


        printf("\nSent result to master.\n");

        //MPI_Send(&comparisons,
        //    1, MPI_LONG, 0, 0,
        //    MPI_COMM_WORLD);

    }


    printf("\n\nFinished");

    MPI_Finalize();


}