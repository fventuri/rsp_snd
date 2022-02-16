/* -*- c++ -*- */
/*
 * Copyright 2021 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_RINGBUFFER_H
#define INCLUDED_RSP_SND_RINGBUFFER_H

#include <condition_variable>
#include <cstddef>
#include <mutex>

template <typename T>
class RingBuffer {

public:
    RingBuffer(size_t size, int verbose = 0);
    ~RingBuffer();

    T* next_write_ptr(size_t advance = 0);
    size_t next_write_max_size();
    T* next_read_ptr(T* current_read_ptr, size_t advance = 0);
    size_t next_read_max_size(T* current_read_ptr, bool blocking = false);
    void stop();

private:
    T* data;
    size_t size;
    size_t write_idx;
    int verbose;
    bool stopped = false;
    size_t max_read_size;
    std::mutex mutex;
    std::condition_variable cv;
};

#endif /* INCLUDED_RSP_SND_RINGBUFFER_H */
