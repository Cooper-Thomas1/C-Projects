#define _POSIX_C_SOURCE     200809L

#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include <regex.h>
#include <getopt.h>
#include <sys/time.h>
#include <utime.h>
#include <unistd.h>


//  CITS2002 Project 2 2023
//  Student1:   23620112   Hugo Smith
//  Student2:   23723986   Cooper Thomas


// GLOBAL PREPROCESSOR CONSTANTS
#define OPTLIST         "ai:no:prv"

// checks if memory allocation has failed and if it has exits with exit failure
#define CHECK_ALLOC(p)  if (p == NULL) { \
                            fprintf(stderr, "Memory allocation failed\n"); \
                            exit(EXIT_FAILURE); }

// activates the print statements only if the verbose option is enabled
#define PRINT_IF_V(fmt, ...) \
    do { \
        if (option->verbose) { \
            printf(fmt, ##__VA_ARGS__); \
        } \
    } while (0)


// STRUCTS AND GLOBAL VARIABLES

typedef struct {
    regex_t **patterns;
    int npatterns;
    bool flag;
} PatternInfo;

typedef struct {
    bool allFiles;              // -a
    bool dryRun;                // -n
    PatternInfo *iPattern;       // -i
    PatternInfo *oPattern;       // -o
    bool preserveAttributes;    // -p
    bool recursive;             // -r
    bool verbose;               // -v
} OPTION;


typedef struct File {
    char *root;                 // File path
    char *local;                // File name
    time_t mtime;               // Modification time
    mode_t permissions;         // File permissions
    struct File *next;          // Pointer to next file
} File;

typedef struct Dir {            
    char *dirpath;              // Directory path
    struct Dir *next;           // Pointer to next directory
} Dir;

extern OPTION *option;
extern File *master;


// GLOBAL FUNCTIONS

extern void initialise_master(char *, int);

extern void updatedir(const char *);

extern char *concat(const char *, const char *);

extern bool is_valid_dir(const char *);

extern char *glob2regex(char *);

extern int matches_only(const char *);

extern int matches_ignore(const char *);