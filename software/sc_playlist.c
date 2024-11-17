#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include "sc_playlist.h"

// Retrieve file at a specific index
struct File *GetFileAtIndex(unsigned int index, struct Folder *FirstFolder) {
    struct Folder *CurrFolder = FirstFolder;
    struct File *CurrFile = FirstFolder->FirstFile;
    bool FoundIt = false;

    printf("Searching for file with index %d\n", index);

    while (!FoundIt) {
        if (CurrFile == NULL) {
            printf("Error: Current file pointer is NULL\n");
            return NULL;
        }

        if (CurrFile->Index == index) {
            printf("File found: %s\n", CurrFile->FullPath);
            return CurrFile;
        } else {
            CurrFile = CurrFile->next;
            if (CurrFile == NULL) {
                CurrFolder = CurrFolder->next;
                if (CurrFolder == NULL) {
                    printf("Error: File not found\n");
                    return NULL;
                }
                CurrFile = CurrFolder->FirstFile;
            }
        }
    }
    return NULL;
}

// Load the file structure
struct Folder *LoadFileStructure(char *BaseFolderPath, unsigned int *TotalNum) {
    struct Folder *prevFold = NULL;
    struct File *prevFile = NULL;

    struct dirent **dirList, **fileList;
    int n, m, o, p;

    struct Folder *FirstFolder = NULL;
    struct File *FirstFile = NULL;

    struct File* new_file;
    struct Folder* new_folder;

    char tempName[257];
    unsigned int FilesCount = 0;
    
    *TotalNum = 0;
    printf("Indexing base folder: %s\n", BaseFolderPath);

    n = scandir(BaseFolderPath, &dirList, 0, alphasort);
    if (n <= 0) {
        printf("Error: Could not scan directory %s or directory is empty.\n", BaseFolderPath);
        return NULL;
    }
    printf("Found %d entries in base directory.\n", n);

    for (o = 0; o < n; o++) {
        printf("Processing entry %d: %s\n", o, dirList[o]->d_name);

        if (dirList[o]->d_name[0] != '.') {
            snprintf(tempName, 257, "%s/%s", BaseFolderPath, dirList[o]->d_name);
            printf("Checking subdirectory: %s\n", tempName);

            m = scandir(tempName, &fileList, 0, alphasort);
            if (m <= 0) {
                printf("Warning: Could not scan subdirectory %s or it is empty.\n", tempName);
                continue;
            }
            printf("Found %d files in subdirectory %s\n", m, tempName);

            FirstFile = NULL;
            prevFile = NULL;

            for (p = 0; p < m; p++) {
                printf("Processing file %d: %s\n", p, fileList[p]->d_name);

                if (fileList[p]->d_name[0] != '.' && strstr(fileList[p]->d_name, ".cue") == NULL) {
                    new_file = (struct File*) malloc(sizeof(struct File));
                    if (!new_file) {
                        printf("Error: Failed to allocate memory for new file.\n");
                        // Handle cleanup if necessary
                        continue;
                    }
                    snprintf(new_file->FullPath, 256, "%s/%s", tempName, fileList[p]->d_name);
                    new_file->Index = FilesCount++;

                    // Link the file in the linked list
                    new_file->prev = prevFile;
                    new_file->next = NULL;
                    if (prevFile) {
                        prevFile->next = new_file;
                    } else {
                        FirstFile = new_file; // Set the first file
                    }
                    prevFile = new_file;

                    printf("Added file %d - %s\n", new_file->Index, new_file->FullPath);
                }
                free(fileList[p]);
            }
            free(fileList);

            if (FirstFile != NULL) {
                new_folder = (struct Folder*) malloc(sizeof(struct Folder));
                if (!new_folder) {
                    printf("Error: Failed to allocate memory for new folder.\n");
                    // Handle cleanup if necessary
                    continue;
                }
                snprintf(new_folder->FullPath, 260, "%s", tempName);

                // Link the folder in the linked list
                new_folder->prev = prevFold;
                new_folder->next = NULL;
                if (prevFold) {
                    prevFold->next = new_folder;
                } else {
                    FirstFolder = new_folder; // Set the first folder
                }
                prevFold = new_folder;

                new_folder->FirstFile = FirstFile;
                printf("Added folder %s with files.\n", new_folder->FullPath);
            }
        }
        free(dirList[o]);
    }
    free(dirList);

    *TotalNum = FilesCount;
    printf("Completed indexing. Total files found: %d\n", FilesCount);
    return FirstFolder;
}

// Dump file structure
void DumpFileStructure(struct Folder *FirstFolder) {
    struct Folder *currFold = FirstFolder;
    struct File *currFile;

    printf("Dumping file structure:\n");

    do {
        if (currFold == NULL) {
            printf("Error: Current folder is NULL\n");
            break;
        }
        printf("Folder: %s\n", currFold->FullPath);

        currFile = currFold->FirstFile;
        if (currFile != NULL) {
            do {
                printf("  File: %s - %s\n", currFold->FullPath, currFile->FullPath);
            } while ((currFile = currFile->next) != NULL);
        }

    } while ((currFold = currFold->next) != NULL);

    printf("File structure dump complete.\n");
}
