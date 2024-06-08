#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void write_output(char *command, char *output);
void read_file_to_shared_memory(const char *filename, char **shared_memory);
void execute_command(char *command);

void write_output(char *command, char *output) {
    FILE *fp;
    fp = fopen("output.txt", "a");
    if (fp == NULL) {
        perror("Failed to open output file");
        exit(1);  // Indicate unsuccessful termination
    }
    fprintf(fp, "The output of: %s : is\n", command);
    fprintf(fp, ">>>>>>>>>>>>>>>\n%s<<<<<<<<<<<<<<<\n", output);
    fclose(fp);
}

void read_file_to_shared_memory(const char *filename, char **shared_memory) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    // get file size
    fseek(file, 0, SEEK_END);
    size_t filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // allocate shared memory and point shared_memory to it
    *shared_memory = mmap(NULL, filesize + 1, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    // check allocation success
    if (*shared_memory == MAP_FAILED) {
        perror("mmap");
        fclose(file);
        exit(1);
    }

    fread(*shared_memory, 1, filesize, file);
    (*shared_memory)[filesize] = '\0';

    fclose(file);
}

void execute_command(char *command) {
    int pipe_file_descriptors[2];

    // check success
    if (pipe(pipe_file_descriptors) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t process_id = fork();

    // check success
    if (process_id == -1) {
        perror("fork");
        exit(1);
    }

    // currently executing the child process
    if (process_id == 0) {
        close(pipe_file_descriptors[0]);
        dup2(pipe_file_descriptors[1], STDOUT_FILENO);
        dup2(pipe_file_descriptors[1], STDERR_FILENO);
        close(pipe_file_descriptors[1]);  // close original write end

        // break up the command string itself
        char *arguments[64];
        char *token = strtok(command, " \n");
        int index = 0;

        while (token != NULL) {
            arguments[index++] = token;
            token = strtok(NULL, " \n");
        }
        arguments[index] = NULL;

        execvp(arguments[0], arguments);  // actually execute the command

        // execvp won't return unless there's an erroor
        perror("execvp");
        exit(1);
    }

    // currently executing the parent process
    else {
        close(pipe_file_descriptors[1]);

        char buffer[1024];
        ssize_t count;
        char output[4096] = {0};
        while ((count = read(pipe_file_descriptors[0], buffer,
                             sizeof(buffer) - 1)) > 0) {
            buffer[count] = '\0';
            strcat(output, buffer);
        }
        close(pipe_file_descriptors[0]);  // Close the read end
        wait(NULL);                       // Wait for child to finish

        write_output(command, output);  // Write the command output to file
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(1);  // failure
    }

    char *shared_memory;
    read_file_to_shared_memory(argv[1], &shared_memory);

    // split into individual commands
    char *command = strtok(shared_memory, "\n");
    while (command != NULL) {
        execute_command(command);      // run the current command
        command = strtok(NULL, "\n");  // get the next command
    }

    if (munmap(shared_memory, strlen(shared_memory) + 1) == -1) {
        perror("munmap");
        exit(1);
    }

    return 0;  // success!!
}
