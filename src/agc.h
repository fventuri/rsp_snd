/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_AGC_H
#define INCLUDED_RSP_SND_AGC_H

#include "rsp.h"

class Agc {

public:
    Agc(int verbose = 0): rsp(nullptr), verbose(verbose) {}
    ~Agc() {}

    // setters
    virtual void setRsp(Rsp* rsp) { this->rsp = rsp; }

    virtual void setup() {}

    // streaming
    virtual void start(RingBuffer<short[2]> *buffer) {}
    virtual void stop() {}

protected:
    Rsp *rsp;
    int verbose;
};

#endif /* INCLUDED_RSP_SND_AGC_H */
