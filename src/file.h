/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_FILE_H
#define INCLUDED_RSP_SND_FILE_H

#include "out.h"
#include "ringbuffer.h"
#include <alsa/asoundlib.h>
#include <stdexcept>
#include <string>
#include <thread>

class FileConfig {
public:
    std::string name;
};

template <typename T>
class File: public Out {

public:
    File(const FileConfig& config, int verbose = 0);
    ~File();

    // streaming
    void start(RingBuffer<T> *buffer) override;
    void stop() override;

    class Exception: public std::runtime_error {
    public:
        Exception(const std::string& reason): std::runtime_error(reason) {}
    };

private:
    void write_loop(RingBuffer<T> *buffer);

    int fd;
    std::thread thread;
    bool run = false;
    size_t total_samples = 0;
};

#endif /* INCLUDED_RSP_SND_FILE_H */
