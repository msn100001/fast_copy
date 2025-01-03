#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/mman.h>
#include <aio.h>
#include <errno.h>
#include <stdarg.h>  // For va_start and va_end
#include <time.h>    // For timestamp
#include <syslog.h>

#define BUFFER_SIZE 65536             // Default buffer size: 64 KB
#define LARGE_FILE_SIZE (100 * 1024 * 1024) // 100 MB threshold for large files
#define MAX_THREADS 8
#define LOG_DIR "/var/log/fast-copy"

typedef struct {
    char source[PATH_MAX];
    char destination[PATH_MAX];
} CopyTask;

// Global counters and file handle
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
int total_files_copied = 0;
FILE *log_file;

// Function prototypes
void setup_logging();
void log_message(const char *format, ...);
int copy_file_sendfile(const char *source, const char *destination);
int copy_file_mmap(const char *source, const char *destination);
int copy_file_aio(const char *source, const char *destination);
int copy_file(const char *source, const char *destination);
void *thread_worker(void *arg);
void traverse_and_copy(const char *source, const char *destination, pthread_t threads[], int *thread_count);

// Initialize logging
void setup_logging() {
    struct stat st;
    if (stat(LOG_DIR, &st) == -1) {
        mkdir(LOG_DIR, 0755);
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char log_filename[PATH_MAX];
    snprintf(log_filename, sizeof(log_filename), "%s/fast-copy-%04d%02d%02d-%02d%02d%02d.log",
             LOG_DIR, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    log_file = fopen(log_filename, "a");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }

    fprintf(log_file, "===== Fast-Copy Execution Log =====\n");
    fflush(log_file);
}

// Log messages to console and log file
void log_message(const char *format, ...) {
    va_list args;

    // Print to console
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // Write to log file
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}

// Dynamically choose the best file copy method
int copy_file(const char *source, const char *destination) {
    struct stat src_stat;
    if (stat(source, &src_stat) != 0) {
        log_message("ERROR: Cannot stat file %s\n", source);
        return 0;
    }

    if (src_stat.st_size > LARGE_FILE_SIZE) {
        return copy_file_sendfile(source, destination);
    }
    if (src_stat.st_size > BUFFER_SIZE) {
        return copy_file_mmap(source, destination);
    }
    return copy_file_aio(source, destination);
}

// Copy large files using sendfile
int copy_file_sendfile(const char *source, const char *destination) {
    int src_fd = open(source, O_RDONLY);
    int dest_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (src_fd < 0 || dest_fd < 0) {
        log_message("ERROR: Failed to open files: %s -> %s\n", source, destination);
        return 0;
    }

    struct stat src_stat;
    fstat(src_fd, &src_stat);

    off_t offset = 0;
    while (offset < src_stat.st_size) {
        ssize_t sent = sendfile(dest_fd, src_fd, &offset, src_stat.st_size - offset);
        if (sent < 0) {
            log_message("ERROR: sendfile failed: %s -> %s\n", source, destination);
            close(src_fd);
            close(dest_fd);
            return 0;
        }
    }

    close(src_fd);
    close(dest_fd);
    log_message("INFO: File copied using sendfile: %s -> %s\n", source, destination);
    return 1;
}

// Copy medium files using mmap
int copy_file_mmap(const char *source, const char *destination) {
    int src_fd = open(source, O_RDONLY);
    int dest_fd = open(destination, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (src_fd < 0 || dest_fd < 0) {
        log_message("ERROR: Failed to open files: %s -> %s\n", source, destination);
        return 0;
    }

    struct stat src_stat;
    fstat(src_fd, &src_stat);
    ftruncate(dest_fd, src_stat.st_size);

    void *src_map = mmap(NULL, src_stat.st_size, PROT_READ, MAP_SHARED, src_fd, 0);
    void *dest_map = mmap(NULL, src_stat.st_size, PROT_WRITE, MAP_SHARED, dest_fd, 0);

    if (src_map == MAP_FAILED || dest_map == MAP_FAILED) {
        log_message("ERROR: mmap failed: %s -> %s\n", source, destination);
        return 0;
    }

    memcpy(dest_map, src_map, src_stat.st_size);
    munmap(src_map, src_stat.st_size);
    munmap(dest_map, src_stat.st_size);
    close(src_fd);
    close(dest_fd);

    log_message("INFO: File copied using mmap: %s -> %s\n", source, destination);
    return 1;
}

// Copy small files using AIO
int copy_file_aio(const char *source, const char *destination) {
    int src_fd = open(source, O_RDONLY);
    int dest_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (src_fd < 0 || dest_fd < 0) {
        log_message("ERROR: Failed to open files: %s -> %s\n", source, destination);
        return 0;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes;
    while ((bytes = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(dest_fd, buffer, bytes) != bytes) {
            log_message("ERROR: Write failed: %s -> %s\n", source, destination);
            close(src_fd);
            close(dest_fd);
            return 0;
        }
    }

    close(src_fd);
    close(dest_fd);
    log_message("INFO: File copied using AIO (basic): %s -> %s\n", source, destination);
    return 1;
}

// Thread worker function
void *thread_worker(void *arg) {
    CopyTask *task = (CopyTask *)arg;
    if (copy_file(task->source, task->destination)) {
        pthread_mutex_lock(&count_mutex);
        total_files_copied++;
        pthread_mutex_unlock(&count_mutex);
    }
    free(task);
    pthread_exit(NULL);
}

// Recursive directory traversal and copying
void traverse_and_copy(const char *source, const char *destination, pthread_t threads[], int *thread_count) {
    DIR *dir = opendir(source);
    if (!dir) return;

    mkdir(destination, 0775);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char src_path[PATH_MAX], dest_path[PATH_MAX];
        snprintf(src_path, PATH_MAX, "%s/%s", source, entry->d_name);
        snprintf(dest_path, PATH_MAX, "%s/%s", destination, entry->d_name);

        struct stat info;
        stat(src_path, &info);

        if (S_ISDIR(info.st_mode)) traverse_and_copy(src_path, dest_path, threads, thread_count);
        else {
            CopyTask *task = malloc(sizeof(CopyTask));
            strncpy(task->source, src_path, PATH_MAX);
            strncpy(task->destination, dest_path, PATH_MAX);
            pthread_create(&threads[(*thread_count)++], NULL, thread_worker, task);

            if (*thread_count == MAX_THREADS) {
                for (int i = 0; i < MAX_THREADS; i++) pthread_join(threads[i], NULL);
                *thread_count = 0;
            }
        }
    }
    closedir(dir);
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    setup_logging();

    pthread_t threads[MAX_THREADS];
    int thread_count = 0;

    log_message("Starting copy process: %s -> %s\n", argv[1], argv[2]);
    traverse_and_copy(argv[1], argv[2], threads, &thread_count);

    for (int i = 0; i < thread_count; i++) pthread_join(threads[i], NULL);

    log_message("Total files copied: %d\n", total_files_copied);
    log_message("Copy process completed.\n");

    fclose(log_file);
    return 0;
}

