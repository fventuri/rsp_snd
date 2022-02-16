/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "agc_gtw.h"
#include "agc_rsp.h"
#include "config.h"
#include "file.h"
#include "out.h"
#include "ringbuffer.h"
#include "rsp.h"
#include "snd.h"
#include <csignal>
#include <iostream>


bool terminate = false;

void terminate_signal_handler(int sig)
{
    terminate = true;
}

int main(int argc, char *argv[])
{
    constexpr size_t RING_BUFFER_SIZE = 65536;

    GlobalConfig global_config;
    RspConfig rsp_config;
    SndConfig snd_config;
    FileConfig file_config;
    AgcRspConfig agc_rsp_config;
    AgcGtwConfig agc_gtw_config;

    get_config(argc, argv, global_config, rsp_config, snd_config, file_config,
               agc_rsp_config, agc_gtw_config);

    Rsp rsp(rsp_config, global_config.verbose);

    Out *out = global_config.isOutFile ?
                   dynamic_cast<Out *>(new File<short[2]>(file_config, global_config.verbose)) :
                   dynamic_cast<Out *>(new Snd(snd_config, global_config.verbose));

    Agc *agc = nullptr;
    if (global_config.agcModel == AGC_RSP)
        agc = new AgcRsp(agc_rsp_config, global_config.verbose);
    if (global_config.agcModel == AGC_GTW)
        agc = new AgcGtw(agc_gtw_config, global_config.verbose);
    if (agc != nullptr) {
        agc->setRsp(&rsp);
        agc->setup();
    }
    
    RingBuffer<short[2]> ringbuffer(RING_BUFFER_SIZE, global_config.verbose);

    rsp.start(&ringbuffer);
    out->start(&ringbuffer);
    if (agc != nullptr)
        agc->start(&ringbuffer);

#if 1
    // handle Ctrl-C and SIGTERM
    signal(SIGINT, terminate_signal_handler);
    signal(SIGTERM, terminate_signal_handler);
    if (isatty(fileno(stderr)))
        std::cerr << "Type ^C to stop" << std::endl;
    struct timespec delay = { 0, 100000000 };   // 100ms delay
    while (!terminate)
        nanosleep(&delay, nullptr);
#else
    struct timespec delay = { 60, 0 };   // 60s delay
    nanosleep(&delay, nullptr);
#endif

    rsp.stop();
    if (agc != nullptr) {
        agc->stop();
        delete agc;
        agc = nullptr;
    }
    out->stop();
    delete out;
    out = nullptr;
    return 0;
}
