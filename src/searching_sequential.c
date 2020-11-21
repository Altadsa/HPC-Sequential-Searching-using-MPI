
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

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

// calculate current time in nanoseconds
long getNanos(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

void runTest()
{

    // we read text at start since there is only 1
    readText();
    printf ("Text length = %d\n\n", textLength);

    // time the entire program
    long timeStart, timeEnd;

    // time individual search
    long patternStart, patternEnd;

    long elapsedNsec;

    int patternNumber = 1;
    int numPatterns = 8;

	// time program start
    timeStart = getNanos();

    for (patternNumber; patternNumber <= numPatterns; patternNumber++)
    {
		// read current pattern
        readPattern(patternNumber);

        printf ("\nPattern %i\n", patternNumber);
        printf ("Pattern length = %d\n", patternLength);

		// perform search
        patternStart = getNanos();
        processData();
        patternEnd = getNanos();

		// calculate elapsed time for this pattern search
        elapsedNsec = (patternEnd - patternStart);

        printf("\nPattern %i elapsed wall clock time = %ld\n", patternNumber, (long)(elapsedNsec / 1.0e9));
        printf("Pattern %i search elapsed CPU time = %.09f\n\n", patternNumber, (double)elapsedNsec / 1.0e9);
    }

    timeEnd = getNanos();

	// calculate total elapsed time for program
    elapsedNsec = (timeEnd - timeStart);
    printf("\n8 Pattern search elapsed wall clock time = %ld\n", (long)(elapsedNsec / 1.0e9));
    printf("8 Pattern search elapsed CPU time = %.09f\n\n", (double)elapsedNsec / 1.0e9);

}


int main()
{
	// run test function carried over from program at the end of assignment 1
    runTest();
}