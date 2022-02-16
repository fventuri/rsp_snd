/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_OUT_H
#define INCLUDED_RSP_SND_OUT_H

#include "ringbuffer.h"

class Out {

public:
    Out(int verbose = 0): verbose(verbose) {}
    ~Out() {}

    // streaming
    virtual void start(RingBuffer<short[2]> *buffer) = 0;
    virtual void stop() = 0;

protected:
    int verbose;
};

#endif /* INCLUDED_RSP_SND_OUT_H */
