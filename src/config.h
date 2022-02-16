/* -*- c++ -*- */
/*
 * Copyright 2022 Franco Venturi.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RSP_SND_CONFIG_H
#define INCLUDED_RSP_SND_CONFIG_H

#include "agc_gtw.h"
#include "file.h"
#include "rsp.h"
#include "snd.h"

enum AgcModel { AGC_NONE, AGC_RSP, AGC_GTW };

typedef struct {
    int verbose;
    bool isOutFile;
    AgcModel agcModel;
} GlobalConfig;

void get_config(int argc, char *const argv[],
                GlobalConfig& global_config, RspConfig& rsp_config,
                SndConfig& snd_config, FileConfig& file_config,
                AgcRspConfig& agc_rsp_config, AgcGtwConfig& agc_gtw_config);

#endif /* INCLUDED_RSP_SND_CONFIG_H */
