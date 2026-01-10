#ifndef JIT_MEMORY_H
#define JIT_MEMORY_H

#include <stddef.h>
#include <stdbool.h>

/**
 * JIT Executable Memory Manager
 * 
 * Provides platform-specific executable memory allocation using mmap.
 * Zero external dependencies - uses POSIX APIs only.
 * 
 * Security: Implements W^X (Write XOR Execute) principle where possible.
 */

/**
 * Allocate executable memory region
 * 
 * @param size Size in bytes (will be rounded up to page size)
 * @return Pointer to allocated memory, or NULL on failure
 * 
 * Memory is initially allocated as RWX (Read-Write-Execute).
 * Use makeExecutable() to change to RX for security.
 */
void* allocateExecutableMemory(size_t size);

/**
 * Free executable memory region
 * 
 * @param ptr Pointer returned by allocateExecutableMemory()
 * @param size Original size passed to allocateExecutableMemory()
 */
void freeExecutableMemory(void* ptr, size_t size);

/**
 * Make memory region executable (read-only)
 * 
 * Changes protection from RW to RX for security (W^X principle).
 * Call this after writing machine code to the region.
 * 
 * @param ptr Pointer to memory region
 * @param size Size of region
 * @return true on success, false on failure
 */
bool makeExecutable(void* ptr, size_t size);

/**
 * Make memory region writable
 * 
 * Changes protection from RX to RW for patching.
 * Use this when you need to modify already-compiled code.
 * 
 * @param ptr Pointer to memory region
 * @param size Size of region
 * @return true on success, false on failure
 */
bool makeWritable(void* ptr, size_t size);

/**
 * Get system page size
 * 
 * @return Page size in bytes (typically 4096)
 */
size_t getPageSize(void);

#endif // JIT_MEMORY_H
