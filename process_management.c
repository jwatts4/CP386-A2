#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void executeCommand(char *command);
void writeOutput(char *command, char *output);
void readFileToSharedMemory(const char *filename, char **sharedMemory);

void executeCommand(char *command) {
    int pipeFileDescriptors[2];
    if (pipe(pipeFileDescriptors) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t processId = fork();
    if (processId == -1) {
        perror("fork");
        exit(1);
    }

    // child process
    if (processId == 0) {
        close(pipeFileDescriptors[0]);                // close unused read end
        dup2(pipeFileDescriptors[1], STDOUT_FILENO);  // redirect stdout to pipe
        dup2(pipeFileDescriptors[1], STDERR_FILENO);  // redirect stderr to pipe
        close(pipeFileDescriptors[1]);  // close the original write end

        // parse the commands
        char *arguments[64];
        char *token = strtok(command, " \n");
        int index = 0;
        while (token != NULL) {
            arguments[index++] = token;
            token = strtok(NULL, " \n");
        }
        arguments[index] = NULL;

        // actually execute
        execvp(arguments[0], arguments);

        // only runs if there was an error
        perror("execvp");
        exit(1);
    }

    // parent process
    else {
        close(pipeFileDescriptors[1]);  // close unused write end

        char buffer[1024];
        ssize_t count;
        char output[4096] = {0};
        while ((count = read(pipeFileDescriptors[0], buffer,
                             sizeof(buffer) - 1)) > 0) {
            buffer[count] = '\0';
            strcat(output, buffer);
        }
        close(pipeFileDescriptors[0]);  // close the read end
        wait(NULL);                     // wait for child to finish

        writeOutput(command, output);  // write the command output to file
    }
}

void writeOutput(char *command, char *output) {
    FILE *filePointer;
    filePointer = fopen("output.txt", "a");
    if (filePointer == NULL) {
        perror("Failed to open output file");
        exit(1);
    }
    fprintf(filePointer, "The output of: %s : is\n", command);
    fprintf(filePointer, ">>>>>>>>>>>>>>>\n%s<<<<<<<<<<<<<<<\n", output);
    fclose(filePointer);
}

// reads the contents of filename and place into a memory location
// accessible by other processes
void readFileToSharedMemory(const char *filename, char **sharedMemory) {
    FILE *filePointer = fopen(filename, "r");
    if (!filePointer) {
        perror("Failed to open file");
        exit(1);
    }

    // move file pointer to the end of the file to determine its size
    fseek(filePointer, 0, SEEK_END);       // SEEK_END goes to eof
    size_t fileSize = ftell(filePointer);  // ftell gets current file position
    fseek(filePointer, 0, SEEK_SET);       // SEEK_SET resets filePointer

    // allocate shared memory using mmap
    // malloc can only allocate memory within a given process
    *sharedMemory = mmap(NULL, fileSize + 1, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (*sharedMemory == MAP_FAILED) {
        perror("mmap");
        fclose(filePointer);
        exit(1);
    }

    // dump whole text file into sharedMemory then add null terminator
    fread(*sharedMemory, 1, fileSize, filePointer);
    (*sharedMemory)[fileSize] = '\0';

    fclose(filePointer);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    char *sharedMemory;
    readFileToSharedMemory(argv[1], &sharedMemory);

    // parse the commands
    char *command = strtok(sharedMemory, "\n");
    while (command != NULL) {
        executeCommand(command);
        command = strtok(NULL, "\n");
    }

    // clean up shared memory
    if (munmap(sharedMemory, strlen(sharedMemory) + 1) == -1) {
        perror("munmap");
        exit(1);
    }

    return 0;
}
