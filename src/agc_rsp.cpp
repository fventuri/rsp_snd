/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "agc_rsp.h"


AgcRsp::AgcRsp(const AgcRspConfig& config, int verbose):
    Agc(verbose),
    mode(config.mode),
    setPoint_dBfs(config.setPoint_dBfs),
    attack_ms(config.attack_ms),
    decay_ms(config.decay_ms),
    decay_delay_ms(config.decay_delay_ms),
    decay_threshold_dB(config.decay_threshold_dB)
{
}

AgcRsp::~AgcRsp()
{
}


void AgcRsp::setup()
{
    rsp->setIFAgc(mode, setPoint_dBfs, attack_ms, decay_ms, decay_delay_ms,
                  decay_threshold_dB);
}
