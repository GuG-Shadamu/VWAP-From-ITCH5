/*
 * @Author: Tairan Gao
 * @Date:   2024-02-10 12:34:19
 * @Last Modified by:   Tairan Gao
 * @Last Modified time: 2024-02-11 15:10:17
 */

#ifndef MEMORY_MAPPED_FILE_READER_H
#define MEMORY_MAPPED_FILE_READER_H

#include <sys/mman.h> // For mmap
#include <sys/stat.h> // For stat
#include <fcntl.h>    // For file constants
#include <unistd.h>   // For close
#include <stdexcept>

namespace GtrTrex {
    class MemoryMappedFileReader {
    public:
        MemoryMappedFileReader(const char* filepath) {
            fd = open(filepath, O_RDONLY);
            if (fd == -1) {
                throw std::runtime_error("Error opening file");
            }

            // Get the size of the file
            struct stat sb;
            if (fstat(fd, &sb) == -1) {
                close(fd);
                throw std::runtime_error("Error getting file size");
            }
            fileSize = sb.st_size;

            // Memory map the file
            mapped = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mapped == MAP_FAILED) {
                close(fd);
                throw std::runtime_error("Error mapping file");
            }
            // File descriptor is no longer needed after the file is mapped
            close(fd);
            fd = -1;
        }

        // Destructor
        ~MemoryMappedFileReader() {
            if (mapped != nullptr) {
                if (munmap(mapped, fileSize) == -1) {
                    std::cerr << "Error un-mapping file\n";
                }
            }
            if (fd != -1) {
                if (close(fd) == -1) {
                    std::cerr << "Error closing file\n";
                }
            }
        }


        MemoryMappedFileReader(const MemoryMappedFileReader&) = delete;
        MemoryMappedFileReader& operator=(const MemoryMappedFileReader&) = delete;

        void* data() const { return mapped; }
        size_t size() const { return fileSize; }

    private:
        void* mapped = nullptr;
        size_t fileSize = 0;
        int fd = -1;
    };
}

#endif
