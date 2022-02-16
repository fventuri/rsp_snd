/* -*- c++ -*- */
/*
 * Copyright 2021 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_SND_H
#define INCLUDED_RSP_SND_SND_H

#include "out.h"
#include "ringbuffer.h"
#include <alsa/asoundlib.h>
#include <stdexcept>
#include <string>
#include <thread>

class SndConfig {
public:
    std::string name;
    double sample_rate;
    unsigned int latency;
};

class Snd: public Out {

public:
    Snd(const SndConfig& config, int verbose = 0);
    ~Snd();

    // streaming
    void start(RingBuffer<short[2]> *buffer) override;
    void stop() override;

    class Exception: public std::runtime_error {
    public:
        Exception(const std::string& reason): std::runtime_error(reason) {}
    };

private:
    void write_loop(RingBuffer<short[2]> *buffer);

    snd_pcm_t *pcm;
    std::thread thread;
    bool run = false;
};

#endif /* INCLUDED_RSP_SND_SND_H */
