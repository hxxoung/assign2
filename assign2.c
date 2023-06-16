#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILENAME_LENGTH 256
#define MAX_METADATA_ENTRIES 100

typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    int size;
} FileMetadata;

typedef struct {
    FileMetadata entries[MAX_METADATA_ENTRIES];
    int count;
} ArchiveMetadata;

void pack(const char* archiveFilename, const char* srcDirectory);
void unpack(const char* archiveFilename, const char* destDirectory);
void add(const char* archiveFilename, const char* targetFilename);
void del(const char* archiveFilename, const char* targetFilename);
void list(const char* archiveFilename);

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <command> <archive-filename> [additional arguments]\n", argv[0]);
        return 1;
    }

    const char* command = argv[1];
    const char* archiveFilename = argv[2];

    if (strcmp(command, "pack") == 0) {
        const char* srcDirectory = argv[3];
        pack(archiveFilename, srcDirectory);
    } else if (strcmp(command, "unpack") == 0) {
        const char* destDirectory = argv[3];
        unpack(archiveFilename, destDirectory);
    } else if (strcmp(command, "add") == 0) {
        const char* targetFilename = argv[3];
        add(archiveFilename, targetFilename);
    } else if (strcmp(command, "del") == 0) {
        const char* targetFilename = argv[3];
        del(archiveFilename, targetFilename);
    } else if (strcmp(command, "list") == 0) {
        list(archiveFilename);
    } else {
        printf("Invalid command: %s\n", command);
        return 1;
    }

    return 0;
}

void pack(const char* archiveFilename, const char* srcDirectory) {
    FILE* archiveFile = fopen(archiveFilename, "wb");
    if (archiveFile == NULL) {
        printf("Error opening archive file: %s\n", archiveFilename);
        return;
    }

    ArchiveMetadata metadata;
    metadata.count = 0;

    // Open source directory
    DIR* srcDir = opendir(srcDirectory);
    if (srcDir == NULL) {
        printf("Error opening source directory: %s\n", srcDirectory);
        fclose(archiveFile);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(srcDir)) != NULL) {
        if (entry->d_type == DT_REG) { // Check if it's a regular file
            // Get file path
            char filePath[MAX_FILENAME_LENGTH];
            snprintf(filePath, sizeof(filePath), "%s/%s", srcDirectory, entry->d_name);

            // Open the file
            FILE* srcFile = fopen(filePath, "rb");
            if (srcFile != NULL) {
                // Get file size
                fseek(srcFile, 0, SEEK_END);
                int fileSize = ftell(srcFile);
                fseek(srcFile, 0, SEEK_SET);

                // Write file metadata to the archive
                if (metadata.count < MAX_METADATA_ENTRIES) {
                    strncpy(metadata.entries[metadata.count].filename, entry->d_name, MAX_FILENAME_LENGTH);
                    metadata.entries[metadata.count].size = fileSize;
                    metadata.count++;
                }

                // Write file contents to the archive
                char buffer[1024];
                size_t bytesRead;
                while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
                    fwrite(buffer, 1, bytesRead, archiveFile);
                }

                fclose(srcFile);
            }
        }
    }

    closedir(srcDir);

    // Write metadata to the archive
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archiveFile);

    fclose(archiveFile);

    printf("Archiving files from %s to %s\n", srcDirectory, archiveFilename);
    printf("%d file(s) archived.\n", metadata.count);
}

void unpack(const char* archiveFilename, const char* destDirectory) {
    FILE* archiveFile = fopen(archiveFilename, "rb");
    if (archiveFile == NULL) {
        printf("Error opening archive file: %s\n", archiveFilename);
        return;
    }

    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archiveFile);

    // Create destination directory if it doesn't exist
    mkdir(destDirectory, 0777);

    char filePath[MAX_FILENAME_LENGTH];

    for (int i = 0; i < metadata.count; i++) {
        FileMetadata fileMetadata = metadata.entries[i];

        // Create the file path in the destination directory
        snprintf(filePath, sizeof(filePath), "%s/%s", destDirectory, fileMetadata.filename);

        // Open the file in the destination directory
        FILE* destFile = fopen(filePath, "wb");
        if (destFile != NULL) {
            // Copy file contents from the archive to the destination file
            char buffer[1024];
            int remainingBytes = fileMetadata.size;
            while (remainingBytes > 0) {
                int bytesToRead = (remainingBytes < sizeof(buffer)) ? remainingBytes : sizeof(buffer);
                size_t bytesRead = fread(buffer, 1, bytesToRead, archiveFile);
                fwrite(buffer, 1, bytesRead, destFile);
                remainingBytes -= bytesRead;
            }

            fclose(destFile);
        }
    }

    fclose(archiveFile);

    printf("Unpacking files from %s to %s\n", archiveFilename, destDirectory);
}

void add(const char* archiveFilename, const char* targetFilename) {
    FILE* archiveFile = fopen(archiveFilename, "r+b");
    if (archiveFile == NULL) {
        printf("Error opening archive file: %s\n", archiveFilename);
        return;
    }

    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archiveFile);

    // Check if the target file already exists in the archive
    for (int i = 0; i < metadata.count; i++) {
        if (strcmp(metadata.entries[i].filename, targetFilename) == 0) {
            printf("Same file name already exists in the archive: %s\n", targetFilename);
            fclose(archiveFile);
            return;
        }
    }

    // Open the target file
    FILE* targetFile = fopen(targetFilename, "rb");
    if (targetFile != NULL) {
        // Get file size
        fseek(targetFile, 0, SEEK_END);
        int fileSize = ftell(targetFile);
        fseek(targetFile, 0, SEEK_SET);

        // Write file metadata to the archive
        if (metadata.count < MAX_METADATA_ENTRIES) {
            strncpy(metadata.entries[metadata.count].filename, targetFilename, MAX_FILENAME_LENGTH);
            metadata.entries[metadata.count].size = fileSize;
            metadata.count++;
        }

        // Write file contents to the archive
        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), targetFile)) > 0) {
            fwrite(buffer, 1, bytesRead, archiveFile);
        }

        fclose(targetFile);
    }

    // Update the metadata in the archive
    fseek(archiveFile, sizeof(ArchiveMetadata) * -1, SEEK_CUR);
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archiveFile);

    fclose(archiveFile);

    printf("Adding file %s to %s\n", targetFilename, archiveFilename);
    printf("1 file added.\n");
}

void del(const char* archiveFilename, const char* targetFilename) {
    FILE* archiveFile = fopen(archiveFilename, "r+b");
    if (archiveFile == NULL) {
        printf("Error opening archive file: %s\n", archiveFilename);
        return;
    }

    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archiveFile);

    // Check if the target file exists in the archive
    int targetIndex = -1;
    for (int i = 0; i < metadata.count; i++) {
        if (strcmp(metadata.entries[i].filename, targetFilename) == 0) {
            targetIndex = i;
            break;
        }
    }

    if (targetIndex == -1) {
        printf("No such file exists in the archive: %s\n", targetFilename);
        fclose(archiveFile);
        return;
    }

    // Remove the file from the metadata
    for (int i = targetIndex; i < metadata.count - 1; i++) {
        metadata.entries[i] = metadata.entries[i + 1];
    }
    metadata.count--;

    // Update the metadata in the archive
    fseek(archiveFile, sizeof(ArchiveMetadata) * -1, SEEK_CUR);
    fwrite(&metadata, sizeof(ArchiveMetadata), 1, archiveFile);

    // Calculate the offset of the target file in the archive
    int offset = sizeof(ArchiveMetadata) + targetIndex * (sizeof(FileMetadata) + metadata.entries[targetIndex].size);

    // Remove the file from the archive by shifting the subsequent data
    int remainingBytes = metadata.entries[targetIndex].size;
    char buffer[1024];
    while (remainingBytes > 0) {
        int bytesRead = fread(buffer, 1, sizeof(buffer), archiveFile);
        fseek(archiveFile, offset, SEEK_SET);
        fwrite(buffer, 1, bytesRead, archiveFile);
        remainingBytes -= bytesRead;
        offset += bytesRead;
    }

    // Truncate the archive file to remove the remaining data
    long fileSize = sizeof(ArchiveMetadata) + metadata.count * sizeof(FileMetadata);
    ftruncate(fileno(archiveFile), fileSize);

    fclose(archiveFile);

    printf("Deleting file %s from %s\n", targetFilename, archiveFilename);
    printf("1 file deleted.\n");
}

void list(const char* archiveFilename) {
    FILE* archiveFile = fopen(archiveFilename, "rb");
    if (archiveFile == NULL) {
        printf("Error opening archive file: %s\n", archiveFilename);
        return;
    }

    ArchiveMetadata metadata;
    fread(&metadata, sizeof(ArchiveMetadata), 1, archiveFile);

    printf("Listing files in %s\n", archiveFilename);
    printf("%d file(s) exist.\n", metadata.count);
    for (int i = 0; i < metadata.count; i++) {
        printf("%s %d byte(s)\n", metadata.entries[i].filename, metadata.entries[i].size);
    }

    fclose(archiveFile);
}
