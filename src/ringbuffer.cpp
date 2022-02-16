/* -*- c++ -*- */
/*
 * Copyright 2021 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ringbuffer.h"
#include <iomanip>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>


template <typename T>
RingBuffer<T>::RingBuffer(size_t size, int verbose):
    data(nullptr),
    size(size),
    write_idx(0),
    verbose(verbose),
    stopped(false),
    max_read_size(0)
{
    const int pagesize = getpagesize();

    size_t bytesize = size * sizeof(T);
    if (bytesize % pagesize != 0)
        throw std::runtime_error("invalid ring buffer size (not a multiple of PAGE_SIZE)");

    auto addr = mmap(NULL, 2 * bytesize, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
        throw std::runtime_error("mmap() failed");
    addr = mremap(addr, 2 * bytesize, bytesize, 0);
    if (addr == MAP_FAILED)
        throw std::runtime_error("mremap() first copy failed");
    auto addr_req = static_cast<unsigned char *>(addr) + bytesize;
    auto addr_ret = mremap(addr, 0, bytesize, MREMAP_FIXED | MREMAP_MAYMOVE, addr_req);
    if (addr_ret == MAP_FAILED)
        throw std::runtime_error("mremap() second copy failed");
    if (addr_ret !=  addr_req)
        throw std::runtime_error("mremap() second copy returned different address than requested");
    data = static_cast<T*>(addr);
}

template <typename T>
RingBuffer<T>::~RingBuffer()
{
    if (data != nullptr) {
        //auto addr = static_cast<unsigned char *>(data);
        auto addr = (unsigned char *) data;
        size_t bytesize = size * sizeof(T);
        munmap(addr + bytesize, bytesize);
        munmap(addr, bytesize);
        data = nullptr;
        size = 0;
        write_idx = 0;
    }
}

template <typename T>
T* RingBuffer<T>::next_write_ptr(size_t advance)
{
    write_idx = (write_idx + advance) % size;
    cv.notify_all();
    return data + write_idx;
}

template <typename T>
size_t RingBuffer<T>::next_write_max_size()
{
    return size - 1;
}

template <typename T>
T* RingBuffer<T>::next_read_ptr(T* current_read_ptr, size_t advance)
{
    if (current_read_ptr == nullptr)
        return data + write_idx;
    size_t read_idx = current_read_ptr - data;
    return data + (read_idx + advance) % size;
}

template <typename T>
size_t RingBuffer<T>::next_read_max_size(T* current_read_ptr, bool blocking)
{
    size_t read_idx = current_read_ptr - data;
    if (blocking) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this, read_idx]() {
            return write_idx != read_idx || stopped;
        });
    }
    size_t read_size = (size + write_idx - read_idx) % size;
    max_read_size = std::max(max_read_size, read_size);
    return read_size;
}

template <typename T>
void RingBuffer<T>::stop()
{
    stopped = true;
    cv.notify_all();
    if (verbose >= 1)
        std::cerr << "ring buffer max_read_size: " << max_read_size << " (" << std::fixed << std::setprecision(2) << (100.0 * max_read_size / size) << "%)" << std::endl;
}


template class RingBuffer<short[2]>;
