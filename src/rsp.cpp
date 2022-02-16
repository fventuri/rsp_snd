/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ringbuffer.h"
#include "rsp.h"
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>


static constexpr int UpdateTimeout = 500;  // wait up to 500ms for updates

static void open_sdrplay_api();
static void static_stream_callback(short *xi, short *xq,
                                   sdrplay_api_StreamCbParamsT *params,
                                   unsigned int numSamples,
                                   unsigned int reset, void *cbContext);
static void static_event_callback(sdrplay_api_EventT eventId,
                                  sdrplay_api_TunerSelectT tuner,
                                  sdrplay_api_EventParamsT *params, void *cbContext);


Rsp::Rsp(const RspConfig& config, int verbose):
    verbose(verbose)
{
    open_sdrplay_api();
    if (verbose >= 1)
        sdrplay_api_DebugEnable(NULL, sdrplay_api_DbgLvl_Verbose);
    select_device(config.serial, config.antenna);
    setSamplerate(config.sample_rate);
    //setBandwidth(config.sample_rate);
    setBandwidth(config.bw_type * 1000.0 + 1);
    setFrequency(config.frequency);
    setIFAgc(sdrplay_api_AGC_DISABLE);
    setIFGainReduction(config.gRdB);
    setRFLnaState(config.lna_state);
    setWideBandSignal(config.wide_band_signal);
    setAntenna(config.antenna);

    gain_file = config.gain_file;
    gain_message = nullptr;
}

Rsp::~Rsp()
{
    if (device_selected) {
        sdrplay_api_LockDeviceApi();
        auto err = sdrplay_api_ReleaseDevice(&device);
        if (err != sdrplay_api_Success)
            std::cerr << "sdrplay_api_ReleaseDevice() failed" << std::endl;
        sdrplay_api_UnlockDeviceApi();
    }
    auto err = sdrplay_api_Close();
    if (err != sdrplay_api_Success)
        std::cerr << "sdrplay_api_Close() failed" << std::endl;
}

void Rsp::select_device(const std::string& serial, const std::string& antenna)
{
    auto err = sdrplay_api_LockDeviceApi();
    if (err != sdrplay_api_Success)
        throw Rsp::Exception("sdrplay_api_LockDeviceApi() failed");

    unsigned int ndevices = SDRPLAY_MAX_DEVICES;
    sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
    err = sdrplay_api_GetDevices(devices, &ndevices, ndevices);
    if (err != sdrplay_api_Success)
        throw Rsp::Exception("sdrplay_api_GetDevices() failed");

    int device_index = 0;
    bool found = false;
    for (int i = 0; i < ndevices; i++) {
        device_index++;
        if (devices[i].hwVer == SDRPLAY_RSPduo_ID) {
            if (select_device_rspduo(devices[i], device_index, serial, antenna)) {
                found = true;
                break;
            }
        } else {
            if (serial.empty() || serial == devices[i].SerNo ||
                                  serial == std::to_string(device_index)) {
                found = true;
                device = devices[i];
                break;
            }
        }
    }

    if (!found)
        throw Rsp::Exception("SDRplay device not found");

    // select the device and get its parameters
    err = sdrplay_api_SelectDevice(&device);
    if (err != sdrplay_api_Success)
        throw Rsp::Exception("sdrplay_api_SelectDevice() failed");
    device_selected = true;
    err = sdrplay_api_UnlockDeviceApi();
    if (err != sdrplay_api_Success)
        throw Rsp::Exception("sdrplay_api_UnlockDeviceApi() failed");
    err = sdrplay_api_GetDeviceParams(device.dev, &device_params);
    if (err != sdrplay_api_Success)
        throw Rsp::Exception("sdrplay_api_GetDeviceParams() failed");
    rx_channel_params = device.tuner != sdrplay_api_Tuner_B ?
                                        device_params->rxChannelA :
                                        device_params->rxChannelB;
    sample_rate = device_params->devParams->fsFreq.fsHz;
    if (device.hwVer == SDRPLAY_RSPduo_ID &&
        device.rspDuoMode != sdrplay_api_RspDuoMode_Single_Tuner)
        sample_rate = 2e6;
    if (rx_channel_params->ctrlParams.decimation.enable)
        sample_rate /= rx_channel_params->ctrlParams.decimation.decimationFactor;

    return;
}

bool Rsp::select_device_rspduo(sdrplay_api_DeviceT& device_rspduo,
                               int device_index,
                               const std::string& serial,
                               const std::string& antenna)
{
    bool found = false;
    if (device_rspduo.rspDuoMode & sdrplay_api_RspDuoMode_Single_Tuner) {
        if (serial.empty() || serial == device_rspduo.SerNo ||
                              serial == std::to_string(device_index)) {
            found = true;
            device = device_rspduo;
            device.rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
        }
    } else if (device_rspduo.rspDuoMode & sdrplay_api_RspDuoMode_Master) {
        if (serial == std::string(device_rspduo.SerNo) + "/M" ||
            serial == std::to_string(device_index) + "/M") {
            found = true;
            device = device_rspduo;
            device.rspDuoMode = sdrplay_api_RspDuoMode_Master;
            device.rspDuoSampleFreq = 6e6;
        } else if (serial == std::string(device_rspduo.SerNo) + "/M8" ||
                   serial == std::to_string(device_index) + "/M8") {
            found = true;
            device = device_rspduo;
            device.rspDuoMode = sdrplay_api_RspDuoMode_Master;
            device.rspDuoSampleFreq = 8e6;
        }
    } else if (device_rspduo.rspDuoMode == sdrplay_api_RspDuoMode_Slave) {
        if (serial.empty() || serial == device_rspduo.SerNo ||
                              serial == std::to_string(device_index)) {
            found = true;
            device = device_rspduo;
        }
    }

    if (!found)
        return found;

    if (device.rspDuoMode == sdrplay_api_RspDuoMode_Single_Tuner ||
        device.rspDuoMode == sdrplay_api_RspDuoMode_Master) {
       device.tuner = sdrplay_api_Tuner_A;
       if (!antenna.empty() && antenna.rfind("Tuner 2", 0) == 0)
           device.tuner = sdrplay_api_Tuner_B;
    }

    return found;
}


// setters
void Rsp::setSamplerate(double sample_rate)
{
    double fsHz = sample_rate;
    unsigned char decimationFactor = 1;
    while (fsHz < 2e6 && decimationFactor <= 32) {
        fsHz *= 2;
        decimationFactor *= 2;
    }
    if (fsHz < 2e6 || fsHz > 10.66e6 || (device.hwVer == SDRPLAY_RSPduo_ID &&
        device.rspDuoMode != sdrplay_api_RspDuoMode_Single_Tuner && fsHz != 2e6))
        throw Rsp::Exception("invalid sample rate");
    sdrplay_api_ReasonForUpdateT reason = sdrplay_api_Update_None;
    if (device_params->devParams && fsHz != device_params->devParams->fsFreq.fsHz) {
        device_params->devParams->fsFreq.fsHz = fsHz;
        reason = (sdrplay_api_ReasonForUpdateT)(reason | sdrplay_api_Update_Dev_Fs);
    }
    sdrplay_api_DecimationT *decimation_params = &rx_channel_params->ctrlParams.decimation;
    if (decimationFactor != decimation_params->decimationFactor) {
        decimation_params->decimationFactor = decimationFactor;
        decimation_params->enable = decimationFactor > 1;
        reason = (sdrplay_api_ReasonForUpdateT)(reason | sdrplay_api_Update_Ctrl_Decimation);
    }

    this->sample_rate = sample_rate;

    return;
}

void Rsp::setBandwidth(double sample_rate)
{
    sdrplay_api_Bw_MHzT bwType = sdrplay_api_BW_Undefined;
    if      (sample_rate <  300e3) { bwType = sdrplay_api_BW_0_200; }
    else if (sample_rate <  600e3) { bwType = sdrplay_api_BW_0_300; }
    else if (sample_rate < 1536e3) { bwType = sdrplay_api_BW_0_600; }
    else if (sample_rate < 5000e3) { bwType = sdrplay_api_BW_1_536; }
    else if (sample_rate < 6000e3) { bwType = sdrplay_api_BW_5_000; }
    else if (sample_rate < 7000e3) { bwType = sdrplay_api_BW_6_000; }
    else if (sample_rate < 8000e3) { bwType = sdrplay_api_BW_7_000; }
    else                          { bwType = sdrplay_api_BW_8_000; }

    rx_channel_params->tunerParams.bwType = bwType;

    return;
}

void Rsp::setFrequency(double frequency)
{
    constexpr double SDRPLAY_FREQ_MIN = 1e3;
    constexpr double SDRPLAY_FREQ_MAX = 2000e6;

    if (frequency < SDRPLAY_FREQ_MIN || frequency > SDRPLAY_FREQ_MAX)
        throw Rsp::Exception("invalid frequency");

    rx_channel_params->tunerParams.rfFreq.rfHz = frequency;

    return;
}

void Rsp::setAntenna(const std::string& antenna)
{
    if (antenna.empty())
        return;

    if (device.hwVer == SDRPLAY_RSP2_ID) {
        sdrplay_api_Rsp2_AntennaSelectT antennaSel;
        sdrplay_api_Rsp2_AmPortSelectT amPortSel;
        if (antenna == "Antenna A") {
            antennaSel = sdrplay_api_Rsp2_ANTENNA_A;
            amPortSel = sdrplay_api_Rsp2_AMPORT_2;
        } else if (antenna == "Antenna B") {
            antennaSel = sdrplay_api_Rsp2_ANTENNA_B;
            amPortSel = sdrplay_api_Rsp2_AMPORT_2;
        } else if (antenna == "Hi-Z") {
            antennaSel = sdrplay_api_Rsp2_ANTENNA_A;
            amPortSel = sdrplay_api_Rsp2_AMPORT_1;
        } else
            throw Rsp::Exception("invalid antenna");
        sdrplay_api_ReasonForUpdateT reason = sdrplay_api_Update_None;
        if (antennaSel != rx_channel_params->rsp2TunerParams.antennaSel) {
            rx_channel_params->rsp2TunerParams.antennaSel = antennaSel;
            reason = (sdrplay_api_ReasonForUpdateT)(reason | sdrplay_api_Update_Rsp2_AntennaControl);
        }
        if (amPortSel != rx_channel_params->rsp2TunerParams.amPortSel) {
            rx_channel_params->rsp2TunerParams.amPortSel = amPortSel;
            reason = (sdrplay_api_ReasonForUpdateT)(reason | sdrplay_api_Update_Rsp2_AmPortSelect);
        }
        return;
    }

    if (device.hwVer == SDRPLAY_RSPduo_ID) {
        sdrplay_api_TunerSelectT tuner;
        sdrplay_api_RspDuo_AmPortSelectT amPortSel;
        if (antenna == "Tuner 1 50ohm") {
            tuner = sdrplay_api_Tuner_A;
            amPortSel = sdrplay_api_RspDuo_AMPORT_2;
        } else if (antenna == "Tuner 2 50ohm") {
            tuner = sdrplay_api_Tuner_B;
            amPortSel = sdrplay_api_RspDuo_AMPORT_2;
        } else if (antenna == "High Z") {
            tuner = sdrplay_api_Tuner_A;
            amPortSel = sdrplay_api_RspDuo_AMPORT_1;
        } else
            throw Rsp::Exception("invalid antenna");
        if (tuner != device.tuner) {
            if (device.rspDuoMode != sdrplay_api_RspDuoMode_Single_Tuner)
                throw Rsp::Exception("invalid antenna in master or slave mode");
            device.tuner = tuner;
            rx_channel_params = device.tuner != sdrplay_api_Tuner_B ?
                                                device_params->rxChannelA :
                                                device_params->rxChannelB;
        }

        rx_channel_params->rspDuoTunerParams.tuner1AmPortSel = amPortSel;

        return;
    }

    if (device.hwVer == SDRPLAY_RSPdx_ID) {
        sdrplay_api_RspDx_AntennaSelectT antennaSel;
        if (antenna == "Antenna A") {
            antennaSel = sdrplay_api_RspDx_ANTENNA_A;
        } else if (antenna == "Antenna B") {
            antennaSel = sdrplay_api_RspDx_ANTENNA_B;
        } else if (antenna == "Antenna C") {
            antennaSel = sdrplay_api_RspDx_ANTENNA_C;
        } else
            throw Rsp::Exception("invalid antenna");

        device_params->devParams->rspDxParams.antennaSel = antennaSel;

        return;
    }

    // can't change antenna for other models
    throw Rsp::Exception("invalid antenna");

    return;
}

void Rsp::setIFGainReduction(int gRdB, bool wait)
{
    if (gRdB < sdrplay_api_NORMAL_MIN_GR || gRdB > MAX_BB_GR)
        throw Rsp::Exception("invalid IF gain reduction");

    sdrplay_api_ReasonForUpdateT reason = sdrplay_api_Update_None;
    if (rx_channel_params->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE) {
        rx_channel_params->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
        reason = (sdrplay_api_ReasonForUpdateT)(reason | sdrplay_api_Update_Ctrl_Agc);
    }
    if (gRdB != rx_channel_params->tunerParams.gain.gRdB) {
        rx_channel_params->tunerParams.gain.gRdB = gRdB;
        reason = (sdrplay_api_ReasonForUpdateT)(reason | sdrplay_api_Update_Tuner_Gr);
    }
    if (run && reason != sdrplay_api_Update_None) {
        gain_reduction_changed = 0;
        auto err = sdrplay_api_Update(device.dev, device.tuner, reason,
                                      sdrplay_api_Update_Ext1_None);
        if (err != sdrplay_api_Success)
            throw Rsp::Exception("sdrplay_api_Update(Ctrl_Agc|Tuner_Gr) failed");

        if (wait && reason & sdrplay_api_Update_Tuner_Gr) {
            for (int i = 0; i < UpdateTimeout && gain_reduction_changed == 0; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (gain_reduction_changed == 0)
                std::cerr << "IF gain reduction update timeout" << std::endl;
        }
    }

    return;
}

void Rsp::setIFAgc(int enable, int setPoint_dBfs,
                   unsigned short attack_ms, unsigned short decay_ms,
                   unsigned short decay_delay_ms, unsigned short decay_threshold_dB,
                   int syncUpdate)
{
    auto& agc = rx_channel_params->ctrlParams.agc;
    agc.enable = static_cast<sdrplay_api_AgcControlT>(enable);
    agc.setPoint_dBfs = setPoint_dBfs;
    agc.attack_ms = attack_ms;
    agc.decay_ms = decay_ms;
    agc.decay_delay_ms = decay_delay_ms;
    agc.decay_threshold_dB = decay_threshold_dB;
    agc.syncUpdate = syncUpdate;
    if (run) {
        auto err = sdrplay_api_Update(device.dev, device.tuner,
                                      sdrplay_api_Update_Ctrl_Agc,
                                      sdrplay_api_Update_Ext1_None);
        if (err != sdrplay_api_Success)
            throw Rsp::Exception("sdrplay_api_Update(Ctrl_Agc) failed");
    }

    return;
}

void Rsp::setRFLnaState(unsigned char LNAstate, bool wait)
{
    // checking for a valid RF LNA state is too complicated since it depends
    // on many factors like the device model, the frequency, etc
    // so I'll assume the user knows what they are doing
    if (LNAstate != rx_channel_params->tunerParams.gain.LNAstate) {
        rx_channel_params->tunerParams.gain.LNAstate = LNAstate;
        if (run) {
            auto err = sdrplay_api_Update(device.dev, device.tuner,
                                          sdrplay_api_Update_Tuner_Gr,
                                          sdrplay_api_Update_Ext1_None);
            if (err != sdrplay_api_Success)
                throw Rsp::Exception("sdrplay_api_Update(Tuner_Gr) failed");
 
            if (wait) {
                for (int i = 0; i < UpdateTimeout && gain_reduction_changed == 0; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (gain_reduction_changed == 0)
                    std::cerr << "RF LNA state update timeout" << std::endl;
            }
        }
    }

    return;
}

void Rsp::setIFType(int if_type)
{
    sdrplay_api_If_kHzT ifType = sdrplay_api_IF_Undefined;
    if      (if_type ==    0) { ifType = sdrplay_api_IF_Zero; }
    else if (if_type ==  450) { ifType = sdrplay_api_IF_0_450; }
    else if (if_type == 1620) { ifType = sdrplay_api_IF_1_620; }
    else if (if_type == 2048) { ifType = sdrplay_api_IF_2_048; }
    else
        throw Rsp::Exception("invalid IF type");
    if (ifType != rx_channel_params->tunerParams.ifType) {
        if (device.hwVer == SDRPLAY_RSPduo_ID &&
            device.rspDuoMode != sdrplay_api_RspDuoMode_Single_Tuner)
            throw Rsp::Exception("invalid IF type in master or slave mode");
        rx_channel_params->tunerParams.ifType = ifType;
    }

    return;
}

void Rsp::setPPM(double ppm)
{
    if (device_params->devParams)
        device_params->devParams->ppm = ppm;

    return;
}

void Rsp::setDCOffset(bool enable)
{
    unsigned char DCenable = enable ? 1 : 0;

    rx_channel_params->ctrlParams.dcOffset.DCenable = DCenable;

    return;
}

void Rsp::setIQBalance(bool enable)
{
    unsigned char IQenable = enable ? 1 : 0;
    if (IQenable)
        rx_channel_params->ctrlParams.dcOffset.DCenable = 1;
    rx_channel_params->ctrlParams.dcOffset.IQenable = IQenable;

    return;
}

void Rsp::setWideBandSignal(bool enable)
{
    unsigned char wideBandSignal = enable ? 1 : 0;
    rx_channel_params->ctrlParams.decimation.wideBandSignal = wideBandSignal;
    return;
}

void Rsp::setBulkTransferMode(bool enable)
{
    sdrplay_api_TransferModeT mode = enable ? sdrplay_api_BULK :
                                              sdrplay_api_ISOCH;
    if (device_params->devParams && mode != device_params->devParams->mode)
        device_params->devParams->mode = mode;

    return;
}


// getters
double Rsp::getSamplerate() const
{
    return sample_rate;
}

int Rsp::getIFGainReduction() const
{
    return rx_channel_params->tunerParams.gain.gRdB;
}


// streaming
void Rsp::start(RingBuffer<short[2]> *buffer)
{
    this->buffer = buffer;
    sdrplay_api_CallbackFnsT callbackFns = {
        static_stream_callback,
        nullptr,
        static_event_callback,
    };

    if (!gain_file.empty())
        open_gain_file();

    auto err = sdrplay_api_Init(device.dev, &callbackFns, this);
    if (err != sdrplay_api_Success)
        throw Rsp::Exception("sdrplay_api_Init() failed");
    total_samples = 0;
    run = true;
}

void Rsp::stop()
{
    if (run) {
        auto err = sdrplay_api_Uninit(device.dev);
        if (err != sdrplay_api_Success)
            throw Rsp::Exception("sdrplay_api_Uninit() failed");
    }
    run = false;

    this->buffer->stop();

    if (!gain_file.empty())
        close_gain_file();

    if (verbose >= 1)
        std::cerr << "rsp source total_samples: " << total_samples << std::endl;
}

static constexpr int MAX_WRITE_TRIES = 3;

void Rsp::stream_callback(short *xi, short *xq,
                          sdrplay_api_StreamCbParamsT *params,
                          unsigned int numSamples,
                          unsigned int reset)
{
    if (!run)
        return;

    gain_reduction_changed |= params->grChanged;

    int xidx = 0;
    auto write_ptr = buffer->next_write_ptr();
    for (int i = 0; i < MAX_WRITE_TRIES; i++) {
        auto max_write_size = buffer->next_write_max_size();
        int samples = std::min((int) max_write_size, (int) numSamples - xidx);
        for (int k = 0; k < samples; k++, xidx++) {
            write_ptr[k][0] = xi[xidx];
            write_ptr[k][1] = xq[xidx];
        }
        write_ptr = buffer->next_write_ptr(samples);
        total_samples += samples;
        if (xidx == numSamples)
            return;
    }

    std::cerr << "stream_callback() - dropped " << (numSamples - xidx) << " samples" << std::endl;

    return;
}

void Rsp::event_callback(sdrplay_api_EventT eventId,
                         sdrplay_api_TunerSelectT tuner,
                         sdrplay_api_EventParamsT *params)
{
    sdrplay_api_GainCbParamT *gainParams = &params->gainParams;
    switch (eventId) {
        case sdrplay_api_GainChange:
            if (gain_message != nullptr) {
                snprintf(gain_message, GAIN_MESSAGE_SIZE,
                         "gRdB=%u\nlnaGRdB=%u\ncurrGain=%lf\n",
                         gainParams->gRdB, gainParams->lnaGRdB, gainParams->currGain);
            }
            break;
        case sdrplay_api_PowerOverloadChange:
            // send ack back for overload events
            if (run) {
                switch (params->powerOverloadParams.powerOverloadChangeType) {
                    case sdrplay_api_Overload_Detected:
                        std::cerr << "overload detected - please reduce gain" << std::endl;
                    break;
                    case sdrplay_api_Overload_Corrected:
                        std::cerr << "overload corrected" << std::endl;
                    break;
                }
                sdrplay_api_Update(device.dev, device.tuner,
                                   sdrplay_api_Update_Ctrl_OverloadMsgAck,
                                   sdrplay_api_Update_Ext1_None);
            }
            break;
        case sdrplay_api_DeviceRemoved:
            std::cerr << "RSP device removed" << std::endl;
            break;
        case sdrplay_api_RspDuoModeChange:
            std::cerr << "RSPduo mode change" << std::endl;
            break;
    }

    return;
}


// internal functions
static void open_sdrplay_api()
{
    auto err = sdrplay_api_Open();
    if (err != sdrplay_api_Success)
        throw Rsp::Exception("sdrplay_api_Open() failed");
    float ver;
    err = sdrplay_api_ApiVersion(&ver);
    if (err != sdrplay_api_Success) {
        sdrplay_api_Close();
        throw Rsp::Exception("sdrplay_api_ApiVersion() failed");
    }
    if (ver != SDRPLAY_API_VERSION) {
        sdrplay_api_Close();
        throw Rsp::Exception("SDRplay API version mismatch");
    }
}

static void static_stream_callback(short *xi, short *xq,
                                   sdrplay_api_StreamCbParamsT *params,
                                   unsigned int numSamples,
                                   unsigned int reset, void *cbContext)
{
    auto rsp = static_cast<Rsp *>(cbContext);
    rsp->stream_callback(xi, xq, params, numSamples, reset);
    return;
}

static void static_event_callback(sdrplay_api_EventT eventId,
                                  sdrplay_api_TunerSelectT tuner,
                                  sdrplay_api_EventParamsT *params, void *cbContext)
{
    auto rsp = static_cast<Rsp *>(cbContext);
    rsp->event_callback(eventId, tuner, params);
    return;
}

void Rsp::open_gain_file()
{
    auto fd = shm_open(gain_file.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        throw Rsp::Exception("shm_open(gain_file) failed");
    if (ftruncate(fd, GAIN_MESSAGE_SIZE) < 0)
        throw Rsp::Exception("ftruncate(gain_file) failed");
    auto addr = mmap(NULL, GAIN_MESSAGE_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        throw Rsp::Exception("mmap(gain_file) failed");
    gain_message = static_cast<char*>(addr);
}

void Rsp::close_gain_file()
{
    if (gain_message != nullptr)
        munmap(gain_message, GAIN_MESSAGE_SIZE);
    if (!gain_file.empty())
        shm_unlink(gain_file.c_str());
    gain_file.clear();
}
