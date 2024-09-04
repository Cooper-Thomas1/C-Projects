#include "mysync.h"

// removes filename from absolute path
void removeFileName(char *path) {
    char *lastSlash = strrchr(path, '/');
    if (lastSlash != NULL) {
        *lastSlash = '\0';
    }
}

// checks if masterfile should replace candidate file
bool should_copy(File *masterfile, const char *rootpath) {
    struct stat root_stat;

    if (stat(rootpath, &root_stat) == 0) {
        if (S_ISREG(root_stat.st_mode)) {

            bool is_older = (masterfile->mtime > root_stat.st_mtime);

            if (!is_older) {
                return false;
            }
        }
    }

    return true;
}

// assigns replacing file's attributes to new file
void preserve_attr(char *rootpath, File *masterfile)
{
    if (chmod(rootpath, masterfile->permissions) != 0) {
        perror("Error changing file permissions");
    }

    struct utimbuf times;
    times.modtime = masterfile->mtime;
    utime(rootpath, &times);
}

// copies file from source to destination
void copy_file(const char *srcPath, const char *destPath) 
{
    FILE *srcFile = fopen(srcPath, "rb");
    if (srcFile == NULL) {
        perror("Failed to open source file");
        return;
    }

    FILE *destFile = fopen(destPath, "wb");
    if (destFile == NULL) {
        perror("Failed to open destination file");
        fclose(srcFile);
        return;
    }

    char buffer[1024];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
        fwrite(buffer, 1, bytesRead, destFile);
    }

    fclose(srcFile);
    fclose(destFile);
}

// updates top level directory
void updatedir(const char *dirpath)
{
    PRINT_IF_V("\nsyncing %s...\n", dirpath);

    File *masterfile = master;

    while (masterfile != NULL) {

        char *rootpath = concat(dirpath, masterfile->local);

        char *fileparent = strdup(rootpath);
        CHECK_ALLOC(fileparent);

        removeFileName(fileparent);

        // check destination directory exists
        if (!is_valid_dir(fileparent) && !option->dryRun) {
            mkdir(fileparent, 0777);
        }

        // check if the destination file exists
        if (should_copy(masterfile, rootpath)) {

            if (!option->dryRun) {
                copy_file(masterfile->root, rootpath);

                if (option->preserveAttributes) {
                    preserve_attr(rootpath, masterfile);
                }
            }

            PRINT_IF_V("\tfile at %s has been copied to %s\n", masterfile->root, rootpath);

            if (option->preserveAttributes) {
                PRINT_IF_V("\t%s has been given %s's attributes\n", rootpath, masterfile->root);
            }
        }
        masterfile = masterfile->next;
    }
}