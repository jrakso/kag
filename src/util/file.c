#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    long pos = ftell(file);
    if (pos < 0) {
        perror("ftell");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    size_t file_size = (size_t)pos;

    if (fseek(file, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    char *buffer = malloc(file_size + 2);  // caller frees
    if (!buffer) {
        perror("malloc");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t read = fread(buffer, 1, file_size, file);
    buffer[read] = '\n';
    buffer[read + 1] = '\0';
    fclose(file);

    return buffer;
}

void write_file(const char *filename, const char *output) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if (fputs(output, f) == EOF) {
        perror("fputs");
        fclose(f);
        exit(EXIT_FAILURE);
    }

    if (fclose(f) == EOF) {
        perror("fclose");
        exit(EXIT_FAILURE);
    }
}