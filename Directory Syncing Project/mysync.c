#include "mysync.h"

OPTION *option = NULL;
File *master = NULL;

// returns new string containing concatenated strings
char *concat(const char *str1, const char *str2) 
{
    size_t len = strlen(str1) + strlen(str2) + 1;

    char *result = (char *)malloc(len);

    if (result) {
        strcpy(result, str1);

        strcat(result, str2);
    }

    return result;
}

// initialises option struct
void initialise_option(void) 
{
    option->allFiles = false;
    option->dryRun = false;

    option->iPattern = malloc(sizeof(PatternInfo));
    CHECK_ALLOC(option->iPattern);
    option->iPattern->npatterns = 0;
    option->iPattern->flag = false;
    option->iPattern->patterns = NULL;

    option->oPattern = malloc(sizeof(PatternInfo));
    CHECK_ALLOC(option->oPattern)
    option->oPattern->npatterns = 0;
    option->oPattern->flag = false;
    option->oPattern->patterns = NULL;

    option->preserveAttributes = false;
    option->recursive = false;
    option->verbose = false;
}

// Function takes a path and checks if it corresponds to a valid directory
bool is_valid_dir(const char *path) 
{
    struct stat info;

    if (stat(path, &info) != 0) {
        return false;
    }

    return S_ISDIR(info.st_mode);
}

// Function processes all command line arguments, checks which options are enabled, processes the remaining directories and add the last directory to a directory list
Dir *process_args(int count, char *args[], Dir *dirlist)
{
    if (count < 3) {
        fprintf(stderr, "Usage: %s [options]  directory1  directory2  [directory3  ...]\n", args[0]);
        exit(EXIT_FAILURE);
    }
    
    int opt;
    opterr = 0;

    char *regexPattern;
    int status;

    // process command line options
    while((opt = getopt(count, args, OPTLIST)) != -1) {
        switch(opt) {
            case 'a':
                option->allFiles = true;
                break;
            case 'i':
                regexPattern = strdup(optarg);
                CHECK_ALLOC(regexPattern);

                regexPattern = glob2regex(regexPattern);

                if (option->iPattern->patterns == NULL) {
                    option->iPattern->patterns = calloc(1, sizeof(regex_t *));
                    CHECK_ALLOC(option->iPattern->patterns);
                }

                option->iPattern->patterns[option->iPattern->npatterns] = malloc(sizeof(regex_t));
                CHECK_ALLOC(option->iPattern->patterns[option->iPattern->npatterns]);

                status = regcomp(option->iPattern->patterns[option->iPattern->npatterns], regexPattern, 0);
                if (status != 0) {
                    perror("couldn't compile regex expression");
                    exit(EXIT_FAILURE);
                }

                option->iPattern->npatterns++;
                option->iPattern->flag = true;
                PRINT_IF_V("include pattern found: %s\n", regexPattern);
                free(regexPattern);

                break;

            case 'n':
                option->dryRun = true;
                option->verbose = true;
                break;
            case 'o':
                regexPattern = strdup(optarg);
                CHECK_ALLOC(regexPattern);

                regexPattern = glob2regex(regexPattern);

                if (option->oPattern->patterns == NULL) {
                    option->oPattern->patterns = calloc(1, sizeof(regex_t));
                    CHECK_ALLOC(option->oPattern->patterns);
                }

                option->oPattern->patterns[option->oPattern->npatterns] = malloc(sizeof(regex_t));
                CHECK_ALLOC(option->oPattern->patterns[option->oPattern->npatterns]);

                status = regcomp(option->oPattern->patterns[option->oPattern->npatterns], regexPattern, 0);
                if (status != 0) {
                    perror("couldn't compile regex expression");
                    exit(EXIT_FAILURE);
                }

                option->oPattern->npatterns++;
                option->oPattern->flag = true;
                PRINT_IF_V("only pattern found: %s\n", regexPattern);
                free(regexPattern);

                break;

            case 'p':
                option->preserveAttributes = true;
                break;
            case 'r':
                option->recursive = true;
                break;
            case 'v':
                option->verbose = true;
                break;
            default:
                count = -1;
        }
    }

    if (count < 0) {
        perror("cannot read command line arguments");
        exit(EXIT_FAILURE);
    }

    count  -= optind;
    args  += optind;

    if (count < 2) {
        perror("Insufficient directories supplied");
        exit(EXIT_FAILURE);
    }

    // find the top level directory paths
    while (count > 0) {
        if (is_valid_dir(*args)) {
            Dir *newdir = (Dir *)malloc(sizeof(Dir));
            newdir->next = NULL;
            newdir->dirpath = strdup(*args);
            
            CHECK_ALLOC(newdir->dirpath);

            if (dirlist == NULL) {
                dirlist = newdir;
            } else {
                Dir *current = dirlist;
                while (current->next != NULL) {
                    current = current->next;
                }
                current->next = newdir;
            }

            count--;
            args++;
        } else {
            perror("Invalid directory path supplied");
            exit(EXIT_FAILURE);
        }
    }
    return dirlist;
}

int main(int argc, char *argv[])
{
    Dir *dirlist = NULL;

    option = malloc(sizeof(OPTION));
    CHECK_ALLOC(option);
    initialise_option();

    dirlist = process_args(argc, argv, dirlist);

    if (argc < 2) {
        perror("Insufficient directories supplied");
        exit(EXIT_FAILURE);
    }

    Dir *currdir = dirlist;


    while (currdir != NULL) {
        if (is_valid_dir(currdir->dirpath)) {

            initialise_master(currdir->dirpath, strlen(currdir->dirpath));

            currdir = currdir->next;
        } else {
            perror("Invalid directory path supplied");
            exit(EXIT_FAILURE);
        }
    }

    // provide the representation of the master directory contents
    if (option->verbose) {
        printf("printing the master blueprint that will be applied to all directories:\n");
        File *current = master;
        while (current != NULL) {
            printf("master file root path: %s\n", current->root);
            current = current->next;
        }
    }

    currdir = dirlist;
    while (currdir != NULL) {
        updatedir(currdir->dirpath);
        currdir = currdir->next;
    }

    PRINT_IF_V("\nsyncing complete\n");
}