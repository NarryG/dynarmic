// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/memory_util.h"

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #include "common/string_util.h"
#else
    #include <cstdlib>
    #include <cstring>
    #include <sys/mman.h>
#endif

#if !defined(_WIN32) && defined(ARCHITECTURE_X64) && !defined(MAP_32BIT)
#include <unistd.h>
#define PAGE_MASK     (getpagesize() - 1)
#define round_page(x) ((((unsigned long)(x)) + PAGE_MASK) & ~(PAGE_MASK))
#endif

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
const char* GetLastErrorMsg()
{
    static const size_t buff_size = 255;

#ifdef _WIN32
    static thread_local char err_str[buff_size] = {};

    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   err_str, buff_size, nullptr);
#else
    static __thread char err_str[buff_size] = {};

    // Thread safe (XSI-compliant)
    strerror_r(errno, err_str, buff_size);
#endif

    return err_str;
}


// This is purposely not a full wrapper for virtualalloc/mmap, but it
// provides exactly the primitive operations that Dolphin needs.

void* AllocateExecutableMemory(size_t size, bool low)
{
#if defined(_WIN32)
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
    static char* map_hint = nullptr;
#if defined(ARCHITECTURE_X64) && !defined(MAP_32BIT)
    // This OS has no flag to enforce allocation below the 4 GB boundary,
    // but if we hint that we want a low address it is very likely we will
    // get one.
    // An older version of this code used MAP_FIXED, but that has the side
    // effect of discarding already mapped pages that happen to be in the
    // requested virtual memory range (such as the emulated RAM, sometimes).
    if (low && (!map_hint))
        map_hint = (char*)round_page(512*1024*1024); /* 0.5 GB rounded up to the next page */
#endif
    void* ptr = mmap(map_hint, size, PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANON | MAP_PRIVATE
#if defined(ARCHITECTURE_X64) && defined(MAP_32BIT)
        | (low ? MAP_32BIT : 0)
#endif
        , -1, 0);
#endif /* defined(_WIN32) */

#ifdef _WIN32
    if (ptr == nullptr)
    {
#else
    if (ptr == MAP_FAILED)
    {
        ptr = nullptr;
#endif
        ASSERT_MSG(false, "Failed to allocate executable memory");
    }
#if !defined(_WIN32) && defined(ARCHITECTURE_X64) && !defined(MAP_32BIT)
    else
    {
        if (low)
        {
            map_hint += size;
            map_hint = (char*)round_page(map_hint); /* round up to the next page */
        }
    }
#endif

#if EMU_ARCH_BITS == 64
    if ((u64)ptr >= 0x80000000 && low == true)
        ASSERT_MSG(false, "Executable memory ended up above 2GB!");
#endif

    return ptr;
}

void* AllocateMemoryPages(size_t size)
{
#ifdef _WIN32
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
#else
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
            MAP_ANON | MAP_PRIVATE, -1, 0);

    if (ptr == MAP_FAILED)
        ptr = nullptr;
#endif

    if (ptr == nullptr)
        ASSERT_MSG(false, "Failed to allocate raw memory");

    return ptr;
}

void* AllocateAlignedMemory(size_t size,size_t alignment)
{
#ifdef _WIN32
    void* ptr =  _aligned_malloc(size,alignment);
#else
    void* ptr = nullptr;
#ifdef ANDROID
    ptr = memalign(alignment, size);
#else
    if (posix_memalign(&ptr, alignment, size) != 0)
        ASSERT_MSG(false, "Failed to allocate aligned memory");
#endif
#endif

    if (ptr == nullptr)
        ASSERT_MSG(false, "Failed to allocate aligned memory");

    return ptr;
}

void FreeMemoryPages(void* ptr, size_t size)
{
    if (ptr)
    {
#ifdef _WIN32
        if (!VirtualFree(ptr, 0, MEM_RELEASE))
            ASSERT_MSG(false, "FreeMemoryPages failed!\n%s", GetLastErrorMsg());
#else
        munmap(ptr, size);
#endif
    }
}

void FreeAlignedMemory(void* ptr)
{
    if (ptr)
    {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
    }
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
#ifdef _WIN32
    DWORD oldValue;
    if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READ : PAGE_READONLY, &oldValue))
        ASSERT_MSG(false, "WriteProtectMemory failed!\n%s", GetLastErrorMsg());
#else
    mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_EXEC) : PROT_READ);
#endif
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
#ifdef _WIN32
    DWORD oldValue;
    if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &oldValue))
        ASSERT_MSG(false, "UnWriteProtectMemory failed!\n%s", GetLastErrorMsg());
#else
    mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_WRITE | PROT_EXEC) : PROT_WRITE | PROT_READ);
#endif
}

std::string MemUsage()
{
    return "";
}
