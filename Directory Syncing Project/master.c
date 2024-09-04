#include "mysync.h"

// copy file information into new struct
File *copy_file_info(File *old)
{
    File *new = malloc(sizeof(File));
    CHECK_ALLOC(new);

    new->next = NULL;
    new->local = strdup(old->local);
    new->root = strdup(old->root);
    new->mtime = old->mtime;
    new->permissions = old->permissions;

    return new;
}

// compare candidate file to master file list
void check_master(File *file) 
{
    File *curr = master;

    if (master == NULL) {
        File *newfile = copy_file_info(file);
        master = newfile;
        return;
    }

    // look for the file in the master list
    while (curr != NULL) {
        if (strcmp(curr->local, file->local) == 0) {
            if (curr->mtime < file->mtime) {
                free(curr->local);
                free(curr->root);

                curr->local = strdup(file->local);
                curr->root = strdup(file->root);
                curr->mtime = file->mtime;
                curr->permissions = file->permissions;
            }
            return;
        }
        curr = curr->next;
    }

    // file not found in master, add it
    File *newfile = copy_file_info(file);

    newfile->next = NULL;
    curr = master;
    while (curr->next != NULL) {
        curr = curr->next;
    }
    curr->next = newfile;
}

// check directory files against master file list
void initialise_master(char *curr_dir_path, int offset)
{
    DIR *dirp;
    struct dirent *dp;

    dirp = opendir(curr_dir_path);

    if (dirp == NULL) {
        perror("Directory not found");
        exit(EXIT_FAILURE);
    }

    while ((dp = readdir(dirp)) != NULL) {

        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }

        if ((dp->d_name[0] == '.') && !option->allFiles) {
            continue;
        }


        // get fullpath of entry
        int fullpath_len = strlen(curr_dir_path) + strlen(dp->d_name) + 2;
        char *fullpath = calloc(1, fullpath_len);
        CHECK_ALLOC(fullpath);
        snprintf(fullpath, fullpath_len, "%s/%s", curr_dir_path, dp->d_name);

        struct stat entry_stat;
        if (stat(fullpath, &entry_stat) == 0) {
            if (S_ISREG(entry_stat.st_mode)) {
                
                // exclude filenames matching ignore pattern
                if (option->iPattern->flag && (matches_ignore(dp->d_name) == 0)) {
                    PRINT_IF_V("%s - file rejected: matches ignorePattern contraint\n", dp->d_name);
                    continue;
                }

                // exclude filenames not matching only pattern
                if (option->oPattern->flag && (matches_only(dp->d_name) != 0)) {
                    PRINT_IF_V("%s - file rejected: fails onlyPattern contraint\n", dp->d_name);
                    continue;
                }
                
                File *file = malloc(sizeof(File)); 
                CHECK_ALLOC(file);
                
                file->root = strdup(fullpath);
                CHECK_ALLOC(file->root);
                
                file->local = file->root + offset;

                struct stat file_stat;
                if (stat(file->root, &file_stat) == 0){
                    file->mtime = file_stat.st_mtime;
                    file->permissions = file_stat.st_mode & 0777;
                } else {
                    fprintf(stderr, "stat failed");
                }

                // check if file needs to be added to master list
                check_master(file);

            } else if (S_ISDIR(entry_stat.st_mode) && option->recursive) {
                
                char *subdirpath = strdup(fullpath);
                CHECK_ALLOC(subdirpath);
                
                // recursively compare master list to subdirectory
                initialise_master(subdirpath, offset);
            }
        } else {
            fprintf(stderr, "stat failed");
        }
    }
    closedir(dirp);
}