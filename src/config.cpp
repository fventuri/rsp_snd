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
#include "rsp.h"
#include "snd.h"
#include <algorithm>
#include <fstream>
#include <getopt.h>
#include <iostream>

static void usage(const char* progname);

static void set_global_config_defaults(GlobalConfig& global_config);
static void set_rsp_config_defaults(RspConfig& rsp_config);
static void set_snd_config_defaults(SndConfig& snd_config);
static void set_file_config_defaults(FileConfig& file_config);
static void set_agc_rsp_config_defaults(AgcRspConfig& agc_rsp_config);
static void set_agc_gtw_config_defaults(AgcGtwConfig& agc_gtw_config);

static void set_unqualified_parameter(const std::string& parameter_name,
                                      const std::string& value,
                                      GlobalConfig& global_config,
                                      RspConfig& rsp_config,
                                      SndConfig& snd_config,
                                      FileConfig& file_config,
                                      AgcRspConfig& agc_rsp_config,
                                      AgcGtwConfig& agc_gtw_config);
static void set_rsp_parameter(const std::string& parameter_name,
                              const std::string& value,
                              RspConfig& rsp_config);
static void set_snd_parameter(const std::string& parameter_name,
                              const std::string& value,
                              SndConfig& snd_config);
static void set_file_parameter(const std::string& parameter_name,
                               const std::string& value,
                               FileConfig& file_config);
static void set_agc_rsp_parameter(const std::string& parameter_name,
                                  const std::string& value,
                                  AgcRspConfig& agc_rsp_config);
static void set_agc_gtw_parameter(const std::string& parameter_name,
                                  const std::string& value,
                                  AgcGtwConfig& agc_gtw_config);


static void read_config_file(const std::string& filename,
                             GlobalConfig& global_config, RspConfig& rsp_config,
                             SndConfig& snd_config, FileConfig& file_config,
                             AgcRspConfig& agc_rsp_config,
                             AgcGtwConfig& agc_gtw_config);

void get_config(int argc, char *const argv[],
                GlobalConfig& global_config, RspConfig& rsp_config,
                SndConfig& snd_config, FileConfig& file_config,
                AgcRspConfig& agc_rsp_config, AgcGtwConfig& agc_gtw_config)
{
    set_global_config_defaults(global_config);
    set_rsp_config_defaults(rsp_config);
    set_snd_config_defaults(snd_config);
    set_file_config_defaults(file_config);
    set_agc_rsp_config_defaults(agc_rsp_config);
    set_agc_gtw_config_defaults(agc_gtw_config);

    std::string out_name;
    double sample_rate;
    int bw_type;

    int c;
    while ((c = getopt(argc, argv, "C:vi:f:r:B:l:We:o:n:a:b:c:g:G:sS:x:y:z:h")) != -1) {
        switch (c) {
            case 'C':
                read_config_file(optarg, global_config, rsp_config, snd_config,
                                 file_config, agc_rsp_config, agc_gtw_config);
                break;
            case 'v':
                global_config.verbose++;
                break;

            // RSP config parameters
            case 'i':
                rsp_config.serial = optarg;
                break;
            case 'f':
                if (sscanf(optarg, "%lf", &rsp_config.frequency) != 1) {
                    std::cerr << "invalid frequency: " << optarg << std::endl;
                    exit(1);
                }
                break;
            case 'r':
                if (sscanf(optarg, "%lf", &sample_rate) != 1) {
                    std::cerr << "invalid sample rate: " << optarg << std::endl;
                    exit(1);
                }
                rsp_config.sample_rate = sample_rate;
                snd_config.sample_rate = sample_rate;
                break;
            case 'B':
                if (sscanf(optarg, "%d", &bw_type) != 1 &&
                    !(bw_type == 200 || bw_type == 300 || bw_type == 600 ||
                      bw_type == 1536 || bw_type == 5000)) {
                    std::cerr << "invalid bandwidth: " << optarg << std::endl;
                    exit(1);
                }
                rsp_config.bw_type = bw_type;
                break;
            case 'l':
                if (sscanf(optarg, "%d", &rsp_config.lna_state) != 1) {
                    std::cerr << "invalid LNA state: " << optarg << std::endl;
                    exit(1);
                }
                break;
            case 'W':
                rsp_config.wide_band_signal = true;
                break;
            case 'e':
                rsp_config.gain_file = optarg;
                break;

            // Output config parameters
            case 'o':
                out_name = optarg;
                break;

            // AGC config parameters
            case 'n':
                if (optarg == "RSP" || optarg == "rsp") {
                    global_config.agcModel = AGC_RSP;
                } else if (optarg == "GTW" || optarg == "gtw") {
                    global_config.agcModel = AGC_GTW;
                } else {
                    std::cerr << "invalid AGC model: " << optarg << std::endl;
                }
                break;
            case 'a':
                if (global_config.agcModel = AGC_RSP)
                    set_agc_rsp_parameter("attack_ms", optarg, agc_rsp_config);
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("agc2_increase_threshold", optarg, agc_gtw_config);
                break;
            case 'b':
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("agc2_decrease_threshold", optarg, agc_gtw_config);
                break;
            case 'c':
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("agc3_min_time_ms", optarg, agc_gtw_config);
                break;
            case 'g':
                if (global_config.agcModel = AGC_RSP)
                    set_agc_rsp_parameter("mode", optarg, agc_rsp_config);
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("min_gain_reduction", optarg, agc_gtw_config);
                break;
            case 'G':
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("max_gain_reduction", optarg, agc_gtw_config);
                break;
            case 's':
                if (global_config.agcModel = AGC_RSP)
                    set_agc_rsp_parameter("setPoint_dBfs", optarg, agc_rsp_config);
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("gainstep_dec", optarg, agc_gtw_config);
                break;
            case 'S':
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("gainstep_inc", optarg, agc_gtw_config);
                break;
            case 'x':
                if (global_config.agcModel = AGC_RSP)
                    set_agc_rsp_parameter("decay_ms", optarg, agc_rsp_config);
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("agc4_a", optarg, agc_gtw_config);
                break;
            case 'y':
                if (global_config.agcModel = AGC_RSP)
                    set_agc_rsp_parameter("decay_delay_ms", optarg, agc_rsp_config);
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("agc5_b", optarg, agc_gtw_config);
                break;
            case 'z':
                if (global_config.agcModel = AGC_RSP)
                    set_agc_rsp_parameter("decay_threshold_dB", optarg, agc_rsp_config);
                if (global_config.agcModel = AGC_GTW)
                    set_agc_gtw_parameter("agc6_c", optarg, agc_gtw_config);
                break;

            // help
            case 'h':
                usage(argv[0]);
                exit(0);
            case '?':
            default:
                usage(argv[0]);
                exit(1);
        }
    }

    global_config.isOutFile = out_name.empty() || out_name == "-" || out_name.find("/") != std::string::npos;
    if (global_config.isOutFile) {
        file_config.name = out_name;
    } else {
        snd_config.name = out_name;
    }
}

static void usage(const char* progname)
{
    std::cerr << "usage: " << progname << " [options...]" << std::endl;
    std::cerr << "options:" << std::endl;
    std::cerr << "    -a attack_ms   (AGC RSP model)" << std::endl;
    std::cerr << "    -a inc   (AGC GTW model) AGC \"increase\" threshold, default 16384" << std::endl;
    std::cerr << "    -B bwType baseband low-pass filter type (200, 300, 600, 1536, 5000)" << std::endl;
    std::cerr << "    -b dec   (AGC GTW model) AGC \"decrease\" threshold, default 8192" << std::endl;
    std::cerr << "    -c min   (AGC GTW model) AGC sample period (ms), default 500" << std::endl;
    std::cerr << "    -e gainfile  write gain values value to shared memory file" << std::endl;
    std::cerr << "    -f freq  set tuner frequency (in Hz)" << std::endl;
    std::cerr << "    -g agc_mode    (AGC RSP model)" << std::endl;
    std::cerr << "    -g gain  (AGC GTW model) set min gain reduction during AGC operation or fixed gain w/AGC disabled, default 30" << std::endl;
    std::cerr << "    -G gain  (AGC GTW model) set max gain reduction during AGC operation, default 59" << std::endl;
    std::cerr << "    -h       show usage" << std::endl;
    std::cerr << "    -i ser   specify input device (serial number)" << std::endl;
    std::cerr << "    -l val   set LNA state, default 3.  See SDRPlay API gain reduction tables for more info" << std::endl;
    std::cerr << "    -n agcmodel  AGC enable; AGC models: RSP, GTW - GTW uses parameters a,b,c,g,s,S,x,y,z" << std::endl;
    std::cerr << "    -o dev   specify output device" << std::endl;
    std::cerr << "    -r rate  set sampling rate (in Hz) [48000, 96000, 192000, 384000, 768000 recommended]" << std::endl;
    std::cerr << "    -S step_inc  (AGC GTW model) set gain AGC attenuation increase (gain reduction) step size in dB, default = 1 (1-10)" << std::endl;
    std::cerr << "    -s setPoint_dBfs   (AGC RSP model)" << std::endl;
    std::cerr << "    -s step_dec  (AGC GTW model) set gain AGC attenuation decrease (gain gain increase) step size in dB, default = 1 (1-10)" << std::endl;
    std::cerr << "    -v       enable verbose output" << std::endl;
    std::cerr << "    -W       enable wideband signal mode (e.g. half-band filtering). Warning: High CPU useage!" << std::endl;
    std::cerr << "    -x decay_ms   (AGC RSP model)" << std::endl;
    std::cerr << "    -x A     (AGC GTW model) num conversions for overload, default 4096" << std::endl;
    std::cerr << "    -y decay_delay_ms   (AGC RSP model)" << std::endl;
    std::cerr << "    -y B     (AGC GTW model) gain decrease event time (ms), default 100" << std::endl;
    std::cerr << "    -z decay_threshold_dB   (AGC RSP model)" << std::endl;
    std::cerr << "    -z C     (AGC GTW model) gain increase event time (ms), default 5000" << std::endl;
}

static void set_global_config_defaults(GlobalConfig& global_config)
{
    global_config.verbose = 0;
    global_config.isOutFile = true;
    global_config.agcModel = AGC_NONE;
}

static void set_rsp_config_defaults(RspConfig& rsp_config)
{
    rsp_config.frequency = 200e6;
    rsp_config.sample_rate = 768e3;
    rsp_config.bw_type = 1536;
    rsp_config.gRdB = 50;
    rsp_config.lna_state = 3;
    rsp_config.wide_band_signal = false;
}

static void set_snd_config_defaults(SndConfig& snd_config)
{
    snd_config.name = "";
    snd_config.sample_rate = 768e3;
    snd_config.latency = 30000;
}

static void set_file_config_defaults(FileConfig& file_config)
{
    file_config.name = "";
}

static void set_agc_rsp_config_defaults(AgcRspConfig& agc_rsp_config)
{
    agc_rsp_config.mode = sdrplay_api_AGC_50HZ;
    agc_rsp_config.setPoint_dBfs = -60;
    agc_rsp_config.attack_ms = 0;
    agc_rsp_config.decay_ms = 0;
    agc_rsp_config.decay_delay_ms = 0;
    agc_rsp_config.decay_threshold_dB = 0;
}

static void set_agc_gtw_config_defaults(AgcGtwConfig& agc_gtw_config)
{
    agc_gtw_config.agc1_increase_threshold = 16384;
    agc_gtw_config.agc2_decrease_threshold = 8192;
    agc_gtw_config.agc3_min_time_ms = 500;
    agc_gtw_config.min_gain_reduction = 30;
    agc_gtw_config.max_gain_reduction = 59;
    agc_gtw_config.gainstep_dec = 1;
    agc_gtw_config.gainstep_inc = 1;
    agc_gtw_config.agc4_a = 4096;
    agc_gtw_config.agc5_b = 1000;
    agc_gtw_config.agc6_c = 5000;
}

static inline void trim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                     [](unsigned char c) { return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                     [](unsigned char c) { return !std::isspace(c); }).base(),
            s.end());
}

void read_config_file(const std::string& filename, GlobalConfig& global_config,
                      RspConfig& rsp_config, SndConfig& snd_config,
                      FileConfig& file_config, AgcRspConfig& agc_rsp_config,
                      AgcGtwConfig& agc_gtw_config)
{
    std::fstream config_file;
    config_file.open(filename, std::ios::in);
    if (!config_file.is_open())
        return;
    std::string line;
    std::string prefix = "";
    while (getline(config_file, line)) {
        if (line[0] == '#')
            continue;
        trim(line);
        if (line.empty())
            continue;
        if (line[0] == '[' && line[line.size()-1] == ']') {
            prefix = line.substr(1, line.size() - 2);
            if (!prefix.empty())
                prefix += ".";
            continue;
        }
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            std::cerr << "invalid config line: " << line << std::endl;
            continue;
        }
        auto key = line.substr(0, pos);
        trim(key);
        auto value = line.substr(pos + 1);
        trim(value);
        auto fullkey = prefix + key;
        std::transform(fullkey.begin(), fullkey.end(), fullkey.begin(), ::tolower);
        pos = fullkey.find('.');
        if (pos == std::string::npos) {
            set_unqualified_parameter(fullkey, value, global_config, rsp_config,
                                      snd_config, file_config, agc_rsp_config,
                                      agc_gtw_config);
        } else {
            auto component = fullkey.substr(0, pos);
            auto parameter_name = fullkey.substr(pos + 1);
            if (component == "rsp") {
                set_rsp_parameter(parameter_name, value, rsp_config);
            } else if (component == "snd") {
                set_snd_parameter(parameter_name, value, snd_config);
            } else if (component == "file") {
                set_file_parameter(parameter_name, value, file_config);
            } else if (component == "agc_rsp") {
                set_agc_rsp_parameter(parameter_name, value, agc_rsp_config);
            } else if (component == "agc_gtw") {
                set_agc_gtw_parameter(parameter_name, value, agc_gtw_config);
            } else {
                std::cerr << "unknown config parameter: " << fullkey << std::endl;
            }
        }
    }
    config_file.close();
    return;
}

static void set_unqualified_parameter(const std::string& parameter_name,
                                      const std::string& value,
                                      GlobalConfig& global_config,
                                      RspConfig& rsp_config,
                                      SndConfig& snd_config,
                                      FileConfig& file_config,
                                      AgcRspConfig& agc_rsp_config,
                                      AgcGtwConfig& agc_gtw_config)
{
    if (parameter_name == "sample_rate") {
        auto sample_rate = strtod(value.c_str(), nullptr);
        rsp_config.sample_rate = sample_rate;
        snd_config.sample_rate = sample_rate;
    } else if (parameter_name == "agc_model") {
        if (value == "RSp" || value == "rsp") {
            global_config.agcModel = AGC_RSP;
        } else if (value == "GTW" || value == "gtw") {
            global_config.agcModel = AGC_GTW;
        } else {
            std::cerr << "invalid AGC model: " << optarg << std::endl;
        }
    } else {
        std::cerr << "invalid unqualified parameter " << parameter_name << std::endl;
    }
}

static void set_rsp_parameter(const std::string& parameter_name,
                              const std::string& value,
                              RspConfig& rsp_config)
{
    if (parameter_name == "serial") {
        rsp_config.serial = value;
    } else if (parameter_name == "frequency") {
        rsp_config.frequency = strtod(value.c_str(), nullptr);
    } else if (parameter_name == "sample_rate") {
        rsp_config.sample_rate = strtod(value.c_str(), nullptr);
    } else if (parameter_name == "bw_type") {
        rsp_config.bw_type = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "gRdB") {
        rsp_config.gRdB = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "lna_state") {
        rsp_config.bw_type = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "wide_band_signal") {
        rsp_config.wide_band_signal = (value == "true" || value == "TRUE");
    } else if (parameter_name == "antenna") {
        rsp_config.antenna = value;
    } else if (parameter_name == "gain_file") {
        rsp_config.gain_file = value;
    } else {
        std::cerr << "invalid rsp parameter " << parameter_name << std::endl;
    }
}

static void set_snd_parameter(const std::string& parameter_name,
                              const std::string& value,
                              SndConfig& snd_config)
{
    if (parameter_name == "name") {
        snd_config.name = value;
    } else if (parameter_name == "sample_rate") {
        snd_config.sample_rate = strtod(value.c_str(), nullptr);
    } else if (parameter_name == "latency") {
        snd_config.latency = static_cast<unsigned int>(strtoul(value.c_str(), nullptr, 10));
    } else {
        std::cerr << "invalid snd parameter " << parameter_name << std::endl;
    }
}

static void set_file_parameter(const std::string& parameter_name,
                               const std::string& value,
                               FileConfig& file_config)
{
    if (parameter_name == "name") {
        file_config.name = value;
    } else {
        std::cerr << "invalid file parameter " << parameter_name << std::endl;
    }
}

static void set_agc_rsp_parameter(const std::string& parameter_name,
                                  const std::string& value,
                                  AgcRspConfig& agc_rsp_config)
{
    if (parameter_name == "mode") {
        if (value.size() == 1) {
            agc_rsp_config.mode = strtol(value.c_str(), nullptr, 10);
        } else if (value == "100HZ" || value == "100Hz") {
            agc_rsp_config.mode = sdrplay_api_AGC_100HZ;
        } else if (value == "50HZ" || value == "50Hz") {
            agc_rsp_config.mode = sdrplay_api_AGC_50HZ;
        } else if (value == "5HZ" || value == "5Hz") {
            agc_rsp_config.mode = sdrplay_api_AGC_5HZ;
        } else if (value == "CTRL_EN" || value == "CTRL_EN") {
            agc_rsp_config.mode = sdrplay_api_AGC_CTRL_EN;
        } else {
            std::cerr << "invalid agc rsp mode " << value << std::endl;
        }
    } else if (parameter_name == "setPoint_dBfs") {
        agc_rsp_config.setPoint_dBfs = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "attack_ms") {
        agc_rsp_config.attack_ms = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "decay_ms") {
        agc_rsp_config.decay_ms = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "decay_delay_ms") {
        agc_rsp_config.decay_delay_ms = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "decay_threshold_dB") {
        agc_rsp_config.decay_threshold_dB = strtol(value.c_str(), nullptr, 10);
    } else {
        std::cerr << "invalid agc rsp parameter " << parameter_name << std::endl;
    }
}

static void set_agc_gtw_parameter(const std::string& parameter_name,
                                  const std::string& value,
                                  AgcGtwConfig& agc_gtw_config)
{
    if (parameter_name == "agc1_increase_threshold") {
        agc_gtw_config.agc1_increase_threshold = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "agc2_decrease_threshold") {
        agc_gtw_config.agc2_decrease_threshold = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "agc3_min_time_ms") {
        agc_gtw_config.agc3_min_time_ms = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "min_gain_reduction") {
        agc_gtw_config.min_gain_reduction = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "max_gain_reduction") {
        agc_gtw_config.max_gain_reduction = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "gainstep_dec") {
        agc_gtw_config.gainstep_dec = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "gainstep_inc") {
        agc_gtw_config.gainstep_inc = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "agc4_a") {
        agc_gtw_config.agc4_a = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "agc5_b") {
        agc_gtw_config.agc5_b = strtol(value.c_str(), nullptr, 10);
    } else if (parameter_name == "agc6_c") {
        agc_gtw_config.agc6_c = strtol(value.c_str(), nullptr, 10);
    } else {
        std::cerr << "invalid agc gtw parameter " << parameter_name << std::endl;
    }
}
