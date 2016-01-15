#ifndef _WWRIFF_H
#define _WWRIFF_H

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <string>
#include <iostream>
#include <fstream>
#include "Bit_stream.h"
#include "stdint.h"
#include "errors.h"

#define VERSION "0.24"

using namespace std;

enum ForcePacketFormat {
    kNoForcePacketFormat,
    kForceModPackets,
    kForceNoModPackets
};


class Wwise_RIFF_Vorbis
{
    string _file_name;
    string _codebooks_name;
    ifstream _infile;
    long _file_size;

    bool _little_endian;

    long _riff_size;
    long _fmt_offset, _cue_offset, _LIST_offset, _smpl_offset, _vorb_offset, _data_offset;
    long _fmt_size, _cue_size, _LIST_size, _smpl_size, _vorb_size, _data_size;

    // RIFF fmt
    uint16_t _channels;
    uint32_t _sample_rate;
    uint32_t _avg_bytes_per_second;

    // RIFF extended fmt
    uint16_t _ext_unk;
    uint32_t _subtype;

    // cue info
    uint32_t _cue_count;

    // smpl info
    uint32_t _loop_count, _loop_start, _loop_end;

    // vorbis info
    uint32_t _sample_count;
    uint32_t _setup_packet_offset;
    uint32_t _first_audio_packet_offset;
    uint32_t _uid;
    uint8_t _blocksize_0_pow;
    uint8_t _blocksize_1_pow;

    const bool _inline_codebooks, _full_setup;
    bool _header_triad_present, _old_packet_headers;
    bool _no_granule, _mod_packets;

    uint16_t (*_read_16)(std::istream &is);
    uint32_t (*_read_32)(std::istream &is);
public:
    Wwise_RIFF_Vorbis(
      const string& name,
      const string& _codebooks_name,
      bool inline_codebooks,
      bool full_setup,
      ForcePacketFormat force_packet_format
      );

    void print_info(void);

    void generate_ogg(ofstream& of);
    void generate_ogg_header(Bit_oggstream& os, bool * & mode_blockflag, int & mode_bits);
    void generate_ogg_header_with_triad(Bit_oggstream& os);
};

#endif
