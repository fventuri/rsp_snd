/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_AGC_GTW_H
#define INCLUDED_RSP_SND_AGC_GTW_H

#include "agc.h"
#include "ringbuffer.h"
#include "rsp.h"
#include <stdexcept>
#include <thread>

class AgcGtwConfig {
public:
    int agc1_increase_threshold; // a (default: 16384)
    int agc2_decrease_threshold; // b (default: 8192)
    int agc3_min_time_ms;        // c (default: 500)
    int min_gain_reduction;      // g (range 20-max_gain_reduction; default: 30)
    int max_gain_reduction;      // G (range min_gain_reduction-59; default: 59)
    int gainstep_dec;            // s (default: 1)
    int gainstep_inc;            // S (default: 1)
    int agc4_a;                  // x (default: 4096)
    int agc5_b;                  // y (default: 1000)
    int agc6_c;                  // z (default: 5000)
};

class AgcGtw: public Agc {

public:
    AgcGtw(const AgcGtwConfig& config, int verbose = 0);
    ~AgcGtw();

    void setup() override;

    // streaming
    void start(RingBuffer<short[2]> *buffer) override;
    void stop() override;

    class Exception: public std::runtime_error {
    public:
        Exception(const std::string& reason): std::runtime_error(reason) {}
    };

private:
    void agc_loop(RingBuffer<short[2]> *buffer);

    std::thread thread;
    bool run = false;

    int gain_reduction;
    int samples_per_millis;
    int samples_left;
    int millis_since_last_agc_check;
    int millis_since_last_gain_change;
    int millis_iq_above_threshold;
    int max_iq;

    int agc1_increase_threshold;
    int agc2_decrease_threshold;
    int agc3_min_time_ms;
    int min_gain_reduction;
    int max_gain_reduction;
    int gainstep_dec;
    int gainstep_inc;
    int agc4_a;
    int agc5_b;
    int agc6_c;
};

#endif /* INCLUDED_RSP_SND_AGC_GTW_H */
