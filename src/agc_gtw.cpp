/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "agc_gtw.h"
#include "ringbuffer.h"
#include <climits>
#include <iostream>


AgcGtw::AgcGtw(const AgcGtwConfig& config, int verbose):
    Agc(verbose),
    agc1_increase_threshold(config.agc1_increase_threshold),
    agc2_decrease_threshold(config.agc2_decrease_threshold),
    agc3_min_time_ms(config.agc3_min_time_ms),
    min_gain_reduction(config.min_gain_reduction),
    max_gain_reduction(config.max_gain_reduction),
    gainstep_dec(config.gainstep_dec),
    gainstep_inc(config.gainstep_inc),
    agc4_a(config.agc4_a),
    agc5_b(config.agc5_b),
    agc6_c(config.agc6_c)
{
}

AgcGtw::~AgcGtw()
{
}


void AgcGtw::setup()
{
    gain_reduction = rsp->getIFGainReduction();
    samples_per_millis = rsp->getSamplerate() / 1000;
    if (verbose >= 1) {
        std::cerr << "enabled AGC GTW with" << std::endl;
        std::cerr << "  AGC1increaseThreshold=" << agc1_increase_threshold << std::endl;
        std::cerr << "  AGC2decreaseThreshold=" << agc2_decrease_threshold << std::endl;
        std::cerr << "  AGC3minTimeMs=" << agc3_min_time_ms << std::endl;
        std::cerr << "  AGC4A=" << agc4_a << std::endl;
        std::cerr << "  AGC5B=" << agc5_b << std::endl;
        std::cerr << "  AGC6C=" << agc6_c << std::endl;
    }
}


// streaming
void AgcGtw::start(RingBuffer<short[2]> *buffer)
{
    samples_left = samples_per_millis;
    millis_since_last_agc_check = 0;
    millis_since_last_gain_change = 0;
    iq2_increase_threshold = agc1_increase_threshold * agc1_increase_threshold;
    iq2_decrease_threshold = agc2_decrease_threshold * agc2_decrease_threshold;
    max_iq2 = 0;
    millis_iq_above_threshold = 0;

    run = true;
    thread = std::thread([this, buffer] { agc_loop(buffer); });
}

void AgcGtw::stop()
{
    if (run) {
        run = false;
        if (thread.joinable())
            thread.join();
    }
}

void AgcGtw::agc_loop(RingBuffer<short[2]> *buffer)
{
    auto read_ptr = buffer->next_read_ptr(nullptr);
    while (run) {
        auto max_read_size = buffer->next_read_max_size(read_ptr, true);

        // AGC logic here
        for (int i = 0; i < max_read_size; ++i) {
            if (--samples_left == 0) {
                millis_since_last_agc_check++;
                millis_since_last_gain_change++;
                samples_left = samples_per_millis;
            }
            int xi = static_cast<int>(read_ptr[i][0]);
            int xq = static_cast<int>(read_ptr[i][1]);
            auto iq2 = xi * xi + xq * xq;

            // high water mark
            max_iq2 = std::max(iq2, max_iq2);

            // how long above high threshold
            if (iq2 > iq2_increase_threshold) {
                // prevent overflow
                if (millis_iq_above_threshold < INT_MAX)
                    millis_iq_above_threshold++;
            }

            // check AGC only after agc3_min_time_ms have elapsed
            if (millis_since_last_agc_check <= agc3_min_time_ms)
                continue;

            if (millis_since_last_gain_change > agc5_b && millis_iq_above_threshold > agc4_a) {
                gain_reduction += gainstep_inc;
                gain_reduction = std::min(max_gain_reduction, gain_reduction);
            } else if (millis_since_last_gain_change > agc6_c && max_iq2 < iq2_increase_threshold) {
                gain_reduction -= gainstep_dec;
                gain_reduction = std::max(min_gain_reduction, gain_reduction);
            }

            millis_since_last_agc_check = 0;
            max_iq2 = 0;
            millis_iq_above_threshold = 0;

            // change IF gain reduction?
            if (gain_reduction != rsp->getIFGainReduction()) {
                if (verbose >= 1)
                    std::cerr << "updating gain_reduction from " << rsp->getIFGainReduction() << " to " << gain_reduction << std::endl;
                rsp->setIFGainReduction(gain_reduction, true);
                millis_since_last_gain_change = 0;
                // reset read_ptr
                read_ptr = buffer->next_read_ptr(nullptr);
                // skip the rest of the samples (they are from before the
                // gain change) and go back to the main loop
                break;
            }
        }

        read_ptr = buffer->next_read_ptr(read_ptr, max_read_size);
    }
}
