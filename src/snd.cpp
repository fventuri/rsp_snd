/* -*- c++ -*- */
/*
 * Copyright 2021 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ringbuffer.h"
#include "snd.h"
#include <iostream>


Snd::Snd(const SndConfig& config, int verbose):
    Out(verbose)
{
    auto err = snd_pcm_open(&pcm, config.name.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "snd_pcm_open(" << config.name << ") failed: " << snd_strerror(err) << std::endl;
        throw Snd::Exception("snd_pcm_open() failed");
    }

    err = snd_pcm_nonblock(pcm, SND_PCM_NONBLOCK);
    if (err < 0) {
        std::cerr << "snd_pcm_nonblock() failed: " << snd_strerror(err) << std::endl;
        throw Snd::Exception("snd_pcm_nonblock() failed");
    }

    err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE,
                             SND_PCM_ACCESS_RW_INTERLEAVED, 2,
                             config.sample_rate, 0, config.latency);
    if (err < 0) {
        std::cerr << "snd_pcm_set_params() failed: " << snd_strerror(err) << std::endl;
        throw Snd::Exception("snd_pcm_set_params() failed");
    }

    err = snd_pcm_prepare(pcm);
    if (err < 0) {
        std::cerr << "snd_pcm_prepare() failed: " << snd_strerror(err) << std::endl;
        throw Snd::Exception("snd_pcm_prepare() failed");
    }
}

Snd::~Snd()
{
    auto err = snd_pcm_close(pcm);
    if (err < 0)
        std::cerr << "snd_pcm_close() failed: " << snd_strerror(err) << std::endl;
}


// streaming
void Snd::start(RingBuffer<short[2]> *buffer)
{
    run = true;
    thread = std::thread([this, buffer] { write_loop(buffer); });
}

void Snd::stop()
{
    if (run) {
        run = false;
        if (thread.joinable())
            thread.join();
    }
}

static constexpr int MAX_WRITEI_TRIES = 4;

void Snd::write_loop(RingBuffer<short[2]> *buffer)
{
    auto read_ptr = buffer->next_read_ptr(nullptr);
    while (run) {
        auto max_read_size = buffer->next_read_max_size(read_ptr, true);
        auto err = snd_pcm_writei(pcm, read_ptr, max_read_size);
        if (err == -EAGAIN) {
            read_ptr = buffer->next_read_ptr(read_ptr, max_read_size);
            continue;
        }
        if (err != -EPIPE)
            std::cerr << "snd_pcm_writei() failed: " << snd_strerror(err) << std::endl;

        err = snd_pcm_prepare(pcm);
        if (err < 0)
            std::cerr << "snd_pcm_prepare() failed: " << snd_strerror(err) << std::endl;
        // try again
        for (int i = 0; i < MAX_WRITEI_TRIES; i++) {
            err = snd_pcm_writei(pcm, read_ptr, max_read_size);
            if (err < 0)
                std::cerr << " snd_pcm_writei() failed: " << snd_strerror(err) << std::endl;
        }
        read_ptr = buffer->next_read_ptr(read_ptr, max_read_size);
    }
}
