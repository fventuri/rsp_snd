/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_RSP_H
#define INCLUDED_RSP_SND_RSP_H

#include "ringbuffer.h"
#include <sdrplay_api.h>
#include <stdexcept>
#include <string>

class RspConfig {
public:
    std::string serial;
    double frequency;
    double sample_rate;
    int bw_type;
    int gRdB;
    int lna_state;
    bool wide_band_signal;
    std::string antenna;
    std::string gain_file;
};

class Rsp {

public:
    Rsp(const RspConfig& config, int verbose = 0);
    ~Rsp();

    // setters
    void setSamplerate(double sample_rate);
    void setBandwidth(double sample_rate);
    void setFrequency(double frequency);
    void setAntenna(const std::string& antenna);
    void setIFGainReduction(int gRdB, bool wait = false);
    void setIFAgc(int enable = sdrplay_api_AGC_50HZ,
                  int setPoint_dBfs = -60,
                  unsigned short attack_ms = 0,
                  unsigned short decay_ms = 0,
                  unsigned short decay_delay_ms = 0,
                  unsigned short decay_threshold_dB = 0,
                  int syncUpdate = 0);
    void setRFLnaState(unsigned char LNAstate, bool wait = false);
    void setIFType(int if_type);
    void setPPM(double ppm);
    void setDCOffset(bool enable);
    void setIQBalance(bool enable);
    void setWideBandSignal(bool enable);
    void setBulkTransferMode(bool enable);

    // getters
    double getSamplerate() const;
    int getIFGainReduction() const;

    // streaming
    void start(RingBuffer<short[2]> *buffer);
    void stop();
    void stream_callback(short *xi, short *xq,
                         sdrplay_api_StreamCbParamsT *params,
                         unsigned int numSamples,
                         unsigned int reset);
    void event_callback(sdrplay_api_EventT eventId,
                        sdrplay_api_TunerSelectT tuner,
                        sdrplay_api_EventParamsT *params);

    class Exception: public std::runtime_error {
    public:
        Exception(const std::string& reason): std::runtime_error(reason) {}
    };

private:
    void select_device(const std::string& serial, const std::string& antenna);
    bool select_device_rspduo(sdrplay_api_DeviceT& device_rspduo,
                              int device_index,
                              const std::string& serial,
                              const std::string& antenna);

    void open_gain_file();
    void close_gain_file();

    sdrplay_api_DeviceT device;
    sdrplay_api_DeviceParamsT *device_params;
    sdrplay_api_RxChannelParamsT *rx_channel_params;
    RingBuffer<short[2]> *buffer;
    double sample_rate;
    int verbose;
    bool run = false;
    bool device_selected = false;
    size_t total_samples = 0;

    int gain_reduction_changed = 0;

    std::string gain_file;
    static constexpr size_t GAIN_MESSAGE_SIZE = 64;
    char *gain_message;
};

#endif /* INCLUDED_RSP_SND_RSP_H */
