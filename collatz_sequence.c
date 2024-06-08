#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char *collatzSequence(int rootNum) {
    char collatzString[1000];  // Should be good enough size
    char dummy[10];
    sprintf(dummy, "%d", rootNum);
    strcpy(collatzString, dummy);
    strcat(collatzString, " ");

    while (rootNum != 1) {  // The actual collatz sequence
        if (rootNum % 2 == 0) {
            rootNum = rootNum / 2;
        } else {
            rootNum = (rootNum * 3) + 1;
        }
        sprintf(dummy, "%d", rootNum);  // Converting variable into string and
                                        // adding to collatzString
        strcat(collatzString, dummy);
        if (rootNum != 1) {
            strcat(collatzString, " ");
        }
    }

    return strdup(collatzString);  // Allocate memory for return string
}

void printMessage(char *collatzString, int initialValue) {
    int size = strlen(collatzString) + 1;
    char *sharedMemory =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,
             0);  // Create shared memory object

    if (sharedMemory == MAP_FAILED) {  // Error handling
        perror("mmap");
        exit(1);
    }

    char initialValueString[10];
    sprintf(initialValueString, "%d", initialValue);

    strcpy(sharedMemory, initialValueString);

    printf("Parent Process: ");
    printf("The positive integer read from file is %s\n", sharedMemory);

    if (fork() == 0) {  // Fork + run conditional if child process
        strcpy(sharedMemory, collatzString);
        printf("Child Process: ");
        printf("The generated collatz sequence is %s \n", sharedMemory);
        munmap(sharedMemory, size);  // Unmap memory in child process
        exit(0);
    }

    wait(NULL);                  // Wait for child process to finish
    munmap(sharedMemory, size);  // Unmap memory in parent process
    return;
}

int main() {
    FILE *pf;
    char number[10];
    int numbers[1000];  // Should be a good enough size in my opinion

    pf = fopen("start_numbers.txt", "r");

    if (pf == NULL) {
        printf("AHHHH error opening file!");
        return 1;
    }

    int i = 0;
    while (fscanf(pf, "%9s", number) == 1) {  // Read numbers until EOF
        int numberInt = atoi(number);
        numbers[i++] = numberInt;
    }
    fclose(pf);

    for (int j = 0; j < i; j++) {
        char *collatzString = collatzSequence(numbers[j]);
        printMessage(collatzString, numbers[j]);
        free(collatzString);  // Free allocated memory
    }

    return 0;
}
