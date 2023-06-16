#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[256];
    int size;
} FileMetadata;

void pack(const char* archiveFilename, const char* srcDirectory) {
    FILE* archiveFile = fopen(archiveFilename, "wb");
    if (archiveFile == NULL) {
        printf("Error creating archive file.\n");
        return;
    }

    // Open the source directory
    DIR* directory = opendir(srcDirectory);
    if (directory == NULL) {
        printf("Error opening source directory.\n");
        fclose(archiveFile);
        return;
    }

    // Write file metadata to archive file
    struct dirent* entry;
    FileMetadata metadata;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_type == DT_REG) {  // Process regular files only
            strcpy(metadata.name, entry->d_name);

            // Get file size
            char filePath[256];
            sprintf(filePath, "%s/%s", srcDirectory, entry->d_name);
            FILE* srcFile = fopen(filePath, "rb");
            if (srcFile == NULL) {
                printf("Error opening source file: %s\n", entry->d_name);
                continue;
            }
            fseek(srcFile, 0L, SEEK_END);
            metadata.size = ftell(srcFile);
            fclose(srcFile);

            fwrite(&metadata, sizeof(FileMetadata), 1, archiveFile);
        }
    }

    closedir(directory);
    fclose(archiveFile);
}

void unpack(const char* archiveFilename, const char* destDirectory) {
    FILE* archiveFile = fopen(archiveFilename, "rb");
    if (archiveFile == NULL) {
        printf("Error opening archive file.\n");
        return;
    }

    // Create destination directory if it doesn't exist
    mkdir(destDirectory, 0755);

    // Read file metadata from archive file and unpack files
    FileMetadata metadata;
    while (fread(&metadata, sizeof(FileMetadata), 1, archiveFile) == 1) {
        char destPath[256];
        sprintf(destPath, "%s/%s", destDirectory, metadata.name);

        FILE* destFile = fopen(destPath, "wb");
        if (destFile == NULL) {
            printf("Error creating destination file: %s\n", metadata.name);
            continue;
        }

        // Copy data from archive file to destination file
        char buffer[1024];
        int bytesRead;
        int totalBytesRead = 0;
        while (totalBytesRead < metadata.size) {
            bytesRead = fread(buffer, 1, sizeof(buffer), archiveFile);
            fwrite(buffer, 1, bytesRead, destFile);
            totalBytesRead += bytesRead;
        }

        fclose(destFile);
    }

    fclose(archiveFile);
}

void add(const char* archiveFilename, const char* targetFilename) {
    FILE* archiveFile = fopen(archiveFilename, "ab");
    if (archiveFile == NULL) {
        printf("Error opening archive file.\n");
        return;
    }

    // Check if the target file already exists in the archive
    FileMetadata metadata;
    fseek(archiveFile, 0L, SEEK_SET);
    while (fread(&metadata, sizeof(FileMetadata), 1, archiveFile) == 1) {
        if (strcmp(metadata.name, targetFilename) == 0) {
            printf("Error: File %s already exists in the archive.\n", targetFilename);
            fclose(archiveFile);
            return;
        }
    }

    // Open the target file
    FILE* targetFile = fopen(targetFilename, "rb");
    if (targetFile == NULL) {
        printf("Error opening target file.\n");
        fclose(archiveFile);
        return;
    }

    // Get file size
    fseek(targetFile, 0L, SEEK_END);
    int fileSize = ftell(targetFile);
    fseek(targetFile, 0L, SEEK_SET);

    // Write file metadata to archive file
    strcpy(metadata.name, targetFilename);
    metadata.size = fileSize;
    fwrite(&metadata, sizeof(FileMetadata), 1, archiveFile);

    // Copy data from target file to archive file
    char buffer[1024];
    int bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), targetFile)) > 0) {
        fwrite(buffer, 1, bytesRead, archiveFile);
    }

    fclose(targetFile);
    fclose(archiveFile);
}

void del(const char* archiveFilename, const char* targetFilename) {
    FILE* archiveFile = fopen(archiveFilename, "rb");
    if (archiveFile == NULL) {
        printf("Error opening archive file.\n");
        return;
    }

    FILE* tempFile = fopen("temp.arc", "wb");
    if (tempFile == NULL) {
        printf("Error creating temporary file.\n");
        fclose(archiveFile);
        return;
    }

    FileMetadata metadata;
    while (fread(&metadata, sizeof(FileMetadata), 1, archiveFile) == 1) {
        if (strcmp(metadata.name, targetFilename) == 0) {
            // Skip this file in the archive
            fseek(archiveFile, metadata.size, SEEK_CUR);
        } else {
            // Write the file metadata and data to the temporary file
            fwrite(&metadata, sizeof(FileMetadata), 1, tempFile);

            char buffer[1024];
            int bytesRead;
            int totalBytesRead = 0;
            while (totalBytesRead < metadata.size) {
                bytesRead = fread(buffer, 1, sizeof(buffer), archiveFile);
                fwrite(buffer, 1, bytesRead, tempFile);
                totalBytesRead += bytesRead;
            }
        }
    }

    fclose(archiveFile);
    fclose(tempFile);

    // Replace the original archive file with the temporary file
    remove(archiveFilename);
    rename("temp.arc", archiveFilename);
}

void list(const char* archiveFilename) {
    FILE* archiveFile = fopen(archiveFilename, "rb");
    if (archiveFile == NULL) {
        printf("Error opening archive file.\n");
        return;
    }

    FileMetadata metadata;
    int fileCount = 0;
    int totalSize = 0;

    while (fread(&metadata, sizeof(FileMetadata), 1, archiveFile) == 1) {
        fileCount++;
        totalSize += metadata.size;
        printf("File Name: %s\n", metadata.name);
        printf("File Size: %d bytes\n", metadata.size);
        printf("-----------------\n");
        fseek(archiveFile, metadata.size, SEEK_CUR);
    }

    printf("Total Files: %d\n", fileCount);
    printf("Total Size: %d bytes\n", totalSize);

    fclose(archiveFile);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Invalid number of arguments.\n");
        return 1;
    }

    char* command = argv[1];
    char* archiveFilename = argv[2];

    if (strcmp(command, "pack") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for pack.\n");
            return 1;
        }
        char* srcDirectory = argv[3];
        pack(archiveFilename, srcDirectory);
    } else if (strcmp(command, "unpack") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for unpack.\n");
            return 1;
        }
        char* destDirectory = argv[3];
        unpack(archiveFilename, destDirectory);
    } else if (strcmp(command, "add") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for add.\n");
            return 1;
        }
        char* targetFilename = argv[3];
        add(archiveFilename, targetFilename);
    } else if (strcmp(command, "del") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for del.\n");
            return 1;
        }
        char* targetFilename = argv[3];
        del(archiveFilename, targetFilename);
    } else if (strcmp(command, "list") == 0) {
        list(archiveFilename);
    } else {
        printf("Invalid command.\n");
        return 1;
    }

    return 0;
}
