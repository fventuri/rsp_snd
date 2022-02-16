/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "file.h"
#include "ringbuffer.h"
#include <fcntl.h>
#include <iostream>


template <typename T>
File<T>::File(const FileConfig& config, int verbose):
    Out(verbose)
{
    if (config.name.empty() || config.name == "-") {
        fd = fileno(stdout);
    } else {
        fd = open(config.name.c_str(), O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            std::cerr << "open(" << config.name << ") failed: " << strerror(errno) << std::endl;
            throw File::Exception("open() failed");
        }
    }
}

template <typename T>
File<T>::~File()
{
    if (fd != fileno(stdout)) {
        auto err = close(fd);
        if (err < 0)
            std::cerr << "close() failed: " << strerror(errno) << std::endl;
    }
}


// streaming
template <typename T>
void File<T>::start(RingBuffer<T> *buffer)
{
    run = true;
    total_samples = 0;
    thread = std::thread([this, buffer] { write_loop(buffer); });
}

template <typename T>
void File<T>::stop()
{
    if (run) {
        run = false;
        if (thread.joinable())
            thread.join();
    }
    if (verbose >= 1)
        std::cerr << "file sink total_samples: " << total_samples << std::endl;
}

template <typename T>
void File<T>::write_loop(RingBuffer<T> *buffer)
{
    auto read_ptr = buffer->next_read_ptr(nullptr);
    while (run) {
        auto max_read_size = buffer->next_read_max_size(read_ptr, true);
        auto bytecount = max_read_size * sizeof(T);
        auto nwritten = write(fd, read_ptr, bytecount);
        if (nwritten < 0)
            std::cerr << "write() failed: " << strerror(errno) << std::endl;
        else if (nwritten != bytecount)
            std::cerr << "write() incomplete - expected: " << bytecount << " - written: " << nwritten << std::endl;
        read_ptr = buffer->next_read_ptr(read_ptr, nwritten / sizeof(T));
        total_samples += nwritten / sizeof(T);
    }
}


template class File<short[2]>;
