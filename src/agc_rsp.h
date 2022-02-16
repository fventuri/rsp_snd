/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_AGC_RSP_H
#define INCLUDED_RSP_SND_AGC_RSP_H

#include "agc.h"
#include "rsp.h"
#include <stdexcept>

class AgcRspConfig {
public:
    int mode;               // g
    int setPoint_dBfs;      // s
    int attack_ms;          // a
    int decay_ms;           // x
    int decay_delay_ms;     // y
    int decay_threshold_dB; // z
};

class AgcRsp: public Agc {

public:
    AgcRsp(const AgcRspConfig& config, int verbose = 0);
    ~AgcRsp();

    void setup() override;

    class Exception: public std::runtime_error {
    public:
        Exception(const std::string& reason): std::runtime_error(reason) {}
    };

private:
    int mode;
    int setPoint_dBfs;
    int attack_ms;
    int decay_ms;
    int decay_delay_ms;
    int decay_threshold_dB;
};

#endif /* INCLUDED_RSP_SND_AGC_RSP_H */
