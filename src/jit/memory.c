#include "jit/memory.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/**
 * JIT Executable Memory Manager Implementation
 * 
 * Uses POSIX mmap for executable memory allocation.
 * Platform: Linux x86-64
 */

size_t getPageSize(void) {
    static size_t pageSize = 0;
    if (pageSize == 0) {
        pageSize = (size_t)sysconf(_SC_PAGESIZE);
    }
    return pageSize;
}

// Round up size to page boundary
static size_t roundUpToPageSize(size_t size) {
    size_t pageSize = getPageSize();
    return ((size + pageSize - 1) / pageSize) * pageSize;
}

void* allocateExecutableMemory(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // Round up to page size
    size_t allocSize = roundUpToPageSize(size);
    
    // Allocate memory with RWX permissions
    // MAP_PRIVATE: Changes are private to this process
    // MAP_ANONYMOUS: Not backed by a file
    void* ptr = mmap(
        NULL,                           // Let kernel choose address
        allocSize,                      // Size (page-aligned)
        PROT_READ | PROT_WRITE | PROT_EXEC,  // RWX permissions
        MAP_PRIVATE | MAP_ANONYMOUS,    // Private, anonymous mapping
        -1,                             // No file descriptor
        0                               // No offset
    );
    
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }
    
    // Zero out the memory for safety
    memset(ptr, 0, allocSize);
    
    return ptr;
}

void freeExecutableMemory(void* ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return;
    }
    
    size_t allocSize = roundUpToPageSize(size);
    
    if (munmap(ptr, allocSize) != 0) {
        perror("munmap failed");
    }
}

bool makeExecutable(void* ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return false;
    }
    
    size_t allocSize = roundUpToPageSize(size);
    
    // Change protection to RX (read-execute, no write)
    // This implements W^X security principle
    if (mprotect(ptr, allocSize, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect (RX) failed");
        return false;
    }
    
    return true;
}

bool makeWritable(void* ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return false;
    }
    
    size_t allocSize = roundUpToPageSize(size);
    
    // Change protection to RW (read-write, no execute)
    if (mprotect(ptr, allocSize, PROT_READ | PROT_WRITE) != 0) {
        perror("mprotect (RW) failed");
        return false;
    }
    
    return true;
}
