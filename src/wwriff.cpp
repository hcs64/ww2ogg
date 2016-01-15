#define __STDC_CONSTANT_MACROS
#include <iostream>
#include <cstring>
#include "stdint.h"
#include "errors.h"
#include "wwriff.h"
#include "Bit_stream.h"
#include "codebook.h"

using namespace std;

/* Modern 2 or 6 byte header */
class Packet
{
    long _offset;
    uint16_t _size;
    uint32_t _absolute_granule;
    bool _no_granule;
public:
    Packet(ifstream& i, long o, bool little_endian, bool no_granule = false) : _offset(o), _size(-1), _absolute_granule(0), _no_granule(no_granule) {
        i.seekg(_offset);

        if (little_endian)
        {
            _size = read_16_le(i);
            if (!_no_granule)
            {
                _absolute_granule = read_32_le(i);
            }
        }
        else
        {
            _size = read_16_be(i);
            if (!_no_granule)
            {
                _absolute_granule = read_32_be(i);
            }
        }
    }

    long header_size(void) { return _no_granule?2:6; }
    long offset(void) { return _offset + header_size(); }
    uint16_t size(void) { return _size; }
    uint32_t granule(void) { return _absolute_granule; }
    long next_offset(void) { return _offset + header_size() + _size; }
};

/* Old 8 byte header */
class Packet_8
{
    long _offset;
    uint32_t _size;
    uint32_t _absolute_granule;
public:
    Packet_8(ifstream& i, long o, bool little_endian) : _offset(o), _size(-1), _absolute_granule(0) {
        i.seekg(_offset);

        if (little_endian)
        {
            _size = read_32_le(i);
            _absolute_granule = read_32_le(i);
        }
        else
        {
            _size = read_32_be(i);
            _absolute_granule = read_32_be(i);
        }
    }

    long header_size(void) { return 8; }
    long offset(void) { return _offset + header_size(); }
    uint32_t size(void) { return _size; }
    uint32_t granule(void) { return _absolute_granule; }
    long next_offset(void) { return _offset + header_size() + _size; }
};

class Vorbis_packet_header
{
    uint8_t type;

    static const char vorbis_str[6];

public:
    explicit Vorbis_packet_header(uint8_t t) : type(t) {}

    friend Bit_oggstream& operator << (Bit_oggstream& bstream, const Vorbis_packet_header& vph) {
        Bit_uint<8> t(vph.type);
        bstream << t;

        for ( unsigned int i = 0; i < 6; i++ )
        {
            Bit_uint<8> c(vorbis_str[i]);
            bstream << c;
        }

        return bstream;
    }
};

const char Vorbis_packet_header::vorbis_str[6] = {'v','o','r','b','i','s'};

Wwise_RIFF_Vorbis::Wwise_RIFF_Vorbis(
    const string& name,
    const string& codebooks_name,
    bool inline_codebooks,
    bool full_setup,
    ForcePacketFormat force_packet_format
    )
  :
    _file_name(name),
    _codebooks_name(codebooks_name),
    _infile(name.c_str(), ios::binary),
    _file_size(-1),
    _little_endian(true),
    _riff_size(-1),
    _fmt_offset(-1),
    _cue_offset(-1),
    _LIST_offset(-1),
    _smpl_offset(-1),
    _vorb_offset(-1),
    _data_offset(-1),
    _fmt_size(-1),
    _cue_size(-1),
    _LIST_size(-1),
    _smpl_size(-1),
    _vorb_size(-1),
    _data_size(-1),
    _channels(0),
    _sample_rate(0),
    _avg_bytes_per_second(0),
    _ext_unk(0),
    _subtype(0),
    _cue_count(0),
    _loop_count(0),
    _loop_start(0),
    _loop_end(0),
    _sample_count(0),
    _setup_packet_offset(0),
    _first_audio_packet_offset(0),
    _uid(0),
    _blocksize_0_pow(0),
    _blocksize_1_pow(0),
    _inline_codebooks(inline_codebooks),
    _full_setup(full_setup),
    _header_triad_present(false),
    _old_packet_headers(false),
    _no_granule(false),
    _mod_packets(false),
    _read_16(NULL),
    _read_32(NULL)
{
    if (!_infile) throw File_open_error(name);

    _infile.seekg(0, ios::end);
    _file_size = _infile.tellg();


    // check RIFF header
    {
        unsigned char riff_head[4], wave_head[4];
        _infile.seekg(0, ios::beg);
        _infile.read(reinterpret_cast<char *>(riff_head), 4);

        if (memcmp(&riff_head[0],"RIFX",4))
        {
            if (memcmp(&riff_head[0],"RIFF",4))
            {
                throw Parse_error_str("missing RIFF");
            }
            else
            {
                _little_endian = true;
            }
        }
        else
        {
            _little_endian = false;
        }

        if (_little_endian)
        {
            _read_16 = read_16_le;
            _read_32 = read_32_le;
        }
        else
        {
            _read_16 = read_16_be;
            _read_32 = read_32_be;
        }

        _riff_size = _read_32(_infile) + 8;

        if (_riff_size > _file_size) throw Parse_error_str("RIFF truncated");

        _infile.read(reinterpret_cast<char *>(wave_head), 4);
        if (memcmp(&wave_head[0],"WAVE",4)) throw Parse_error_str("missing WAVE");
    }

    // read chunks
    long chunk_offset = 12;
    while (chunk_offset < _riff_size)
    {
        _infile.seekg(chunk_offset, ios::beg);

        if (chunk_offset + 8 > _riff_size) throw Parse_error_str("chunk header truncated");

        char chunk_type[4];
        _infile.read(chunk_type, 4);
        uint32_t chunk_size;
        
        chunk_size = _read_32(_infile);

        if (!memcmp(chunk_type,"fmt ",4))
        {
            _fmt_offset = chunk_offset + 8;
            _fmt_size = chunk_size;
        }
        else if (!memcmp(chunk_type,"cue ",4))
        {
            _cue_offset = chunk_offset + 8;
            _cue_size = chunk_size;
        }
        else if (!memcmp(chunk_type,"LIST",4))
        {
            _LIST_offset = chunk_offset + 8;
            _LIST_size = chunk_size;
        }
        else if (!memcmp(chunk_type,"smpl",4))
        {
            _smpl_offset = chunk_offset + 8;
            _smpl_size = chunk_size;
        }
        else if (!memcmp(chunk_type,"vorb",4))
        {
            _vorb_offset = chunk_offset + 8;
            _vorb_size = chunk_size;
        }
        else if (!memcmp(chunk_type,"data",4))
        {
            _data_offset = chunk_offset + 8;
            _data_size = chunk_size;
        }

        chunk_offset = chunk_offset + 8 + chunk_size;
    }

    if (chunk_offset > _riff_size) throw Parse_error_str("chunk truncated");

    // check that we have the chunks we're expecting
    if (-1 == _fmt_offset && -1 == _data_offset) throw Parse_error_str("expected fmt, data chunks");

    // read fmt
    if (-1 == _vorb_offset && 0x42 != _fmt_size) throw Parse_error_str("expected 0x42 fmt if vorb missing");

    if (-1 != _vorb_offset && 0x28 != _fmt_size && 0x18 != _fmt_size && 0x12 != _fmt_size) throw Parse_error_str("bad fmt size");

    if (-1 == _vorb_offset && 0x42 == _fmt_size)
    {
        // fake it out
        _vorb_offset = _fmt_offset + 0x18;
    }

    _infile.seekg(_fmt_offset, ios::beg);
    if (UINT16_C(0xFFFF) != _read_16(_infile)) throw Parse_error_str("bad codec id");
    _channels = _read_16(_infile);
    _sample_rate = _read_32(_infile);
    _avg_bytes_per_second = _read_32(_infile);
    if (0U != _read_16(_infile)) throw Parse_error_str("bad block align");
    if (0U != _read_16(_infile)) throw Parse_error_str("expected 0 bps");
    if (_fmt_size-0x12 != _read_16(_infile)) throw Parse_error_str("bad extra fmt length");

    if (_fmt_size-0x12 >= 2) {
      // read extra fmt
      _ext_unk = _read_16(_infile);
      if (_fmt_size-0x12 >= 6) {
        _subtype = _read_32(_infile);
      }
    }

    if (_fmt_size == 0x28)
    {
        char whoknowsbuf[16];
        const unsigned char whoknowsbuf_check[16] = {1,0,0,0, 0,0,0x10,0, 0x80,0,0,0xAA, 0,0x38,0x9b,0x71};
        _infile.read(whoknowsbuf, 16);
        if (memcmp(whoknowsbuf, whoknowsbuf_check, 16)) throw Parse_error_str("expected signature in extra fmt?");
    }

    // read cue
    if (-1 != _cue_offset)
    {
#if 0
        if (0x1c != _cue_size) throw Parse_error_str("bad cue size");
#endif
        _infile.seekg(_cue_offset);

        _cue_count = _read_32(_infile);
    }
    
    // read LIST
    if (-1 != _LIST_offset)
    {
#if 0
        if ( 4 != _LIST_size ) throw Parse_error_str("bad LIST size");
        char adtlbuf[4];
        const char adtlbuf_check[4] = {'a','d','t','l'};
        _infile.seekg(_LIST_offset);
        _infile.read(adtlbuf, 4);
        if (memcmp(adtlbuf, adtlbuf_check, 4)) throw Parse_error_str("expected only adtl in LIST");
#endif
    }

    // read smpl
    if (-1 != _smpl_offset)
    {
        _infile.seekg(_smpl_offset+0x1C);
        _loop_count = _read_32(_infile);

        if (1 != _loop_count) throw Parse_error_str("expected one loop");

        _infile.seekg(_smpl_offset+0x2c);
        _loop_start = _read_32(_infile);
        _loop_end = _read_32(_infile);
    }

    // read vorb
    switch (_vorb_size)
    {
        case -1:
        case 0x28:
        case 0x2A:
        case 0x2C:
        case 0x32:
        case 0x34:
            _infile.seekg(_vorb_offset+0x00, ios::beg);
            break;

        default:
            throw Parse_error_str("bad vorb size");
            break;
    }

    _sample_count = _read_32(_infile);

    switch (_vorb_size)
    {
        case -1:
        case 0x2A:
        {
            _no_granule = true;

            _infile.seekg(_vorb_offset + 0x4, ios::beg);
            uint32_t mod_signal = _read_32(_infile);

            // set
            // D9     11011001
            // CB     11001011
            // BC     10111100
            // B2     10110010
            // unset
            // 4A     01001010
            // 4B     01001011
            // 69     01101001
            // 70     01110000
            // A7     10100111 !!!

            // seems to be 0xD9 when _mod_packets should be set
            // also seen 0xCB, 0xBC, 0xB2
            if (0x4A != mod_signal && 0x4B != mod_signal && 0x69 != mod_signal && 0x70 != mod_signal)
            {
                _mod_packets = true;
            }
            _infile.seekg(_vorb_offset + 0x10, ios::beg);
            break;
        }

        default:
            _infile.seekg(_vorb_offset + 0x18, ios::beg);
            break;
    }

    if (force_packet_format == kForceNoModPackets)
    {
        _mod_packets = false;
    }
    else if (force_packet_format == kForceModPackets)
    {
        _mod_packets = true;
    }

    _setup_packet_offset = _read_32(_infile);
    _first_audio_packet_offset = _read_32(_infile);

    switch (_vorb_size)
    {
        case -1:
        case 0x2A:
            _infile.seekg(_vorb_offset + 0x24, ios::beg);
            break;

        case 0x32:
        case 0x34:
            _infile.seekg(_vorb_offset + 0x2C, ios::beg);
            break;
    } 

    switch(_vorb_size)
    {
        case 0x28:
        case 0x2C:
            // ok to leave _uid, _blocksize_0_pow and _blocksize_1_pow unset
            _header_triad_present = true;
            _old_packet_headers = true;
            break;

        case -1:
        case 0x2A:
        case 0x32:
        case 0x34:
            _uid = _read_32(_infile);
            _blocksize_0_pow = _infile.get();
            _blocksize_1_pow = _infile.get();
            break;
    }

    // check/set loops now that we know total sample count
    if (0 != _loop_count)
    {
        if (_loop_end == 0)
        {
            _loop_end = _sample_count;
        }
        else
        {
            _loop_end = _loop_end + 1;
        }

        if (_loop_start >= _sample_count || _loop_end > _sample_count || _loop_start > _loop_end)
            throw Parse_error_str("loops out of range");
    }

    // check subtype now that we know the vorb info
    // this is clearly just the channel layout
    switch (_subtype)
    {
        case 4:     /* 1 channel, no seek table */
        case 3:     /* 2 channels */
        case 0x33:  /* 4 channels */
        case 0x37:  /* 5 channels, seek or not */
        case 0x3b:  /* 5 channels, no seek table */
        case 0x3f:  /* 6 channels, no seek table */
            break;
        default:
            //throw Parse_error_str("unknown subtype");
            break;
    }
}

void Wwise_RIFF_Vorbis::print_info(void)
{
    if (_little_endian)
    {
        cout << "RIFF WAVE";
    }
    else
    {
        cout << "RIFX WAVE";
    }
    cout << " " << _channels << " channel";
    if (_channels != 1) cout << "s";
    cout << " " << _sample_rate << " Hz " << _avg_bytes_per_second*8 << " bps" << endl;
    cout << _sample_count << " samples" << endl;

    if (0 != _loop_count)
    {
        cout << "loop from " << _loop_start << " to " << _loop_end << endl;
    }

    if (_old_packet_headers)
    {
        cout << "- 8 byte (old) packet headers" << endl;
    }
    else if (_no_granule)
    {
        cout << "- 2 byte packet headers, no granule" << endl;
    }
    else
    {
        cout << "- 6 byte packet headers" << endl;
    }

    if (_header_triad_present)
    {
        cout << "- Vorbis header triad present" << endl;
    }

    if (_full_setup || _header_triad_present)
    {
        cout << "- full setup header" << endl;
    }
    else
    {
        cout << "- stripped setup header" << endl;
    }

    if (_inline_codebooks || _header_triad_present)
    {
        cout << "- inline codebooks" << endl;
    }
    else
    {
        cout << "- external codebooks (" << _codebooks_name << ")" << endl;
    }

    if (_mod_packets)
    {
        cout << "- modified Vorbis packets" << endl;
    }
    else
    {
        cout << "- standard Vorbis packets" << endl;
    }

#if 0
    if (0 != _cue_count)
    {
        cout << _cue_count << " cue point";
        if (_cue_count != 1) cout << "s";
        cout << endl;
    }
#endif
}

void Wwise_RIFF_Vorbis::generate_ogg_header(Bit_oggstream& os, bool * & mode_blockflag, int & mode_bits)
{
    // generate identification packet
    {
        Vorbis_packet_header vhead(1);

        os << vhead;

        Bit_uint<32> version(0);
        os << version;

        Bit_uint<8> ch(_channels);
        os << ch;

        Bit_uint<32> srate(_sample_rate);
        os << srate;

        Bit_uint<32> bitrate_max(0);
        os << bitrate_max;

        Bit_uint<32> bitrate_nominal(_avg_bytes_per_second * 8);
        os << bitrate_nominal;

        Bit_uint<32> bitrate_minimum(0);
        os << bitrate_minimum;

        Bit_uint<4> blocksize_0(_blocksize_0_pow);
        os << blocksize_0;

        Bit_uint<4> blocksize_1(_blocksize_1_pow);
        os << blocksize_1;

        Bit_uint<1> framing(1);
        os << framing;

        // identification packet on its own page
        os.flush_page();
    }

    // generate comment packet
    {
        Vorbis_packet_header vhead(3);

        os << vhead;

        static const char vendor[] = "converted from Audiokinetic Wwise by ww2ogg " VERSION;
        Bit_uint<32> vendor_size(strlen(vendor));

        os << vendor_size;
        for (unsigned int i = 0; i < vendor_size; i ++) {
            Bit_uint<8> c(vendor[i]);
            os << c;
        }

        if (0 == _loop_count)
        {
            // no user comments
            Bit_uint<32> user_comment_count(0);
            os << user_comment_count;
        }
        else
        {
            // two comments, loop start and end
            Bit_uint<32> user_comment_count(2);
            os << user_comment_count;

            stringstream loop_start_str;
            stringstream loop_end_str;
            
            loop_start_str << "LoopStart=" << _loop_start;
            loop_end_str << "LoopEnd=" << _loop_end;

            Bit_uint<32> loop_start_comment_length;
            loop_start_comment_length = loop_start_str.str().length();
            os << loop_start_comment_length;
            for (unsigned int i = 0; i < loop_start_comment_length; i++)
            {
                Bit_uint<8> c(loop_start_str.str().c_str()[i]);
                os << c;
            }

            Bit_uint<32> loop_end_comment_length;
            loop_end_comment_length = loop_end_str.str().length();
            os << loop_end_comment_length;
            for (unsigned int i = 0; i < loop_end_comment_length; i++)
            {
                Bit_uint<8> c(loop_end_str.str().c_str()[i]);
                os << c;
            }
        }

        Bit_uint<1> framing(1);
        os << framing;

        //os.flush_bits();
        os.flush_page();
    }

    // generate setup packet
    {
        Vorbis_packet_header vhead(5);

        os << vhead;

        Packet setup_packet(_infile, _data_offset + _setup_packet_offset, _little_endian, _no_granule);

        _infile.seekg(setup_packet.offset());
        if (setup_packet.granule() != 0) throw Parse_error_str("setup packet granule != 0");
        Bit_stream ss(_infile);

        // codebook count
        Bit_uint<8> codebook_count_less1;
        ss >> codebook_count_less1;
        unsigned int codebook_count = codebook_count_less1 + 1;
        os << codebook_count_less1;

        //cout << codebook_count << " codebooks" << endl;

        // rebuild codebooks
        if (_inline_codebooks)
        {
            codebook_library cbl;

            for (unsigned int i = 0; i < codebook_count; i++)
            {
                if (_full_setup)
                {
                    cbl.copy(ss, os);
                }
                else
                {
                    cbl.rebuild(ss, 0, os);
                }
            }
        }
        else
        {
            /* external codebooks */

            codebook_library cbl(_codebooks_name);

            for (unsigned int i = 0; i < codebook_count; i++)
            {
                Bit_uint<10> codebook_id;
                ss >> codebook_id;
                //cout << "Codebook " << i << " = " << codebook_id << endl;
                try
                {
                    cbl.rebuild(codebook_id, os);
                }
                catch (Invalid_id e)
                {
                    //         B         C         V
                    //    4    2    4    3    5    6
                    // 0100 0010 0100 0011 0101 0110
                    // \_______|____ ___|/
                    //              X
                    //            11 0100 0010

                    if (codebook_id == 0x342)
                    {
                        Bit_uint<14> codebook_identifier;
                        ss >> codebook_identifier;

                        //         B         C         V
                        //    4    2    4    3    5    6
                        // 0100 0010 0100 0011 0101 0110
                        //           \_____|_ _|_______/
                        //                   X
                        //         01 0101 10 01 0000
                        if (codebook_identifier == 0x1590)
                        {
                            // starts with BCV, probably --full-setup
                            throw Parse_error_str(
                                "invalid codebook id 0x342, try --full-setup");
                        }
                    }

                    // just an invalid codebook
                    throw e;
                }
            }
        }

        // Time Domain transforms (placeholder)
        Bit_uint<6> time_count_less1(0);
        os << time_count_less1;
        Bit_uint<16> dummy_time_value(0);
        os << dummy_time_value;

        if (_full_setup)
        {

            while (ss.get_total_bits_read() < setup_packet.size()*8u)
            {
                Bit_uint<1> bitly;
                ss >> bitly;
                os << bitly;
            }
        }
        else    // _full_setup
        {
            // floor count
            Bit_uint<6> floor_count_less1;
            ss >> floor_count_less1;
            unsigned int floor_count = floor_count_less1 + 1;
            os << floor_count_less1;

            // rebuild floors
            for (unsigned int i = 0; i < floor_count; i++)
            {
                // Always floor type 1
                Bit_uint<16> floor_type(1);
                os << floor_type;

                Bit_uint<5> floor1_partitions;
                ss >> floor1_partitions;
                os << floor1_partitions;

                unsigned int * floor1_partition_class_list = new unsigned int [floor1_partitions];

                unsigned int maximum_class = 0;
                for (unsigned int j = 0; j < floor1_partitions; j++)
                {
                    Bit_uint<4> floor1_partition_class;
                    ss >> floor1_partition_class;
                    os << floor1_partition_class;

                    floor1_partition_class_list[j] = floor1_partition_class;

                    if (floor1_partition_class > maximum_class)
                        maximum_class = floor1_partition_class;
                }

                unsigned int * floor1_class_dimensions_list = new unsigned int [maximum_class+1];

                for (unsigned int j = 0; j <= maximum_class; j++)
                {
                    Bit_uint<3> class_dimensions_less1;
                    ss >> class_dimensions_less1;
                    os << class_dimensions_less1;

                    floor1_class_dimensions_list[j] = class_dimensions_less1 + 1;

                    Bit_uint<2> class_subclasses;
                    ss >> class_subclasses;
                    os << class_subclasses;

                    if (0 != class_subclasses)
                    {
                        Bit_uint<8> masterbook;
                        ss >> masterbook;
                        os << masterbook;

                        if (masterbook >= codebook_count)
                            throw Parse_error_str("invalid floor1 masterbook");
                    }

                    for (unsigned int k = 0; k < (1U<<class_subclasses); k++)
                    {
                        Bit_uint<8> subclass_book_plus1;
                        ss >> subclass_book_plus1;
                        os << subclass_book_plus1;

                        int subclass_book = static_cast<int>(subclass_book_plus1)-1;
                        if (subclass_book >= 0 && static_cast<unsigned int>(subclass_book) >= codebook_count)
                            throw Parse_error_str("invalid floor1 subclass book");
                    }
                }

                Bit_uint<2> floor1_multiplier_less1;
                ss >> floor1_multiplier_less1;
                os << floor1_multiplier_less1;

                Bit_uint<4> rangebits;
                ss >> rangebits;
                os << rangebits;

                for (unsigned int j = 0; j < floor1_partitions; j++)
                {
                    unsigned int current_class_number = floor1_partition_class_list[j];
                    for (unsigned int k = 0; k < floor1_class_dimensions_list[current_class_number]; k++)
                    {
                        Bit_uintv X(rangebits);
                        ss >> X;
                        os << X;
                    }
                }

                delete [] floor1_class_dimensions_list;
                delete [] floor1_partition_class_list;
            }

            // residue count
            Bit_uint<6> residue_count_less1;
            ss >> residue_count_less1;
            unsigned int residue_count = residue_count_less1 + 1;
            os << residue_count_less1;

            // rebuild residues
            for (unsigned int i = 0; i < residue_count; i++)
            {
                Bit_uint<2> residue_type;
                ss >> residue_type;
                os << Bit_uint<16>(residue_type);

                if (residue_type > 2) throw Parse_error_str("invalid residue type");

                Bit_uint<24> residue_begin, residue_end, residue_partition_size_less1;
                Bit_uint<6> residue_classifications_less1;
                Bit_uint<8> residue_classbook;

                ss >> residue_begin >> residue_end >> residue_partition_size_less1 >> residue_classifications_less1 >> residue_classbook;
                unsigned int residue_classifications = residue_classifications_less1 + 1;
                os << residue_begin << residue_end << residue_partition_size_less1 << residue_classifications_less1 << residue_classbook;

                if (residue_classbook >= codebook_count) throw Parse_error_str("invalid residue classbook");

                unsigned int * residue_cascade = new unsigned int [residue_classifications];

                for (unsigned int j = 0; j < residue_classifications; j++)
                {
                    Bit_uint<5> high_bits(0);
                    Bit_uint<3> low_bits;

                    ss >> low_bits;
                    os << low_bits;

                    Bit_uint<1> bitflag;
                    ss >> bitflag;
                    os << bitflag;
                    if (bitflag)
                    {
                        ss >> high_bits;
                        os << high_bits;
                    }

                    residue_cascade[j] = high_bits * 8 + low_bits;
                }

                for (unsigned int j = 0; j < residue_classifications; j++)
                {
                    for (unsigned int k = 0; k < 8; k++)
                    {
                        if (residue_cascade[j] & (1 << k))
                        {
                            Bit_uint<8> residue_book;
                            ss >> residue_book;
                            os << residue_book;

                            if (residue_book >= codebook_count) throw Parse_error_str("invalid residue book");
                        }
                    }
                }

                delete [] residue_cascade;
            }

            // mapping count
            Bit_uint<6> mapping_count_less1;
            ss >> mapping_count_less1;
            unsigned int mapping_count = mapping_count_less1 + 1;
            os << mapping_count_less1;

            for (unsigned int i = 0; i < mapping_count; i++)
            {
                // always mapping type 0, the only one
                Bit_uint<16> mapping_type(0);

                os << mapping_type;

                Bit_uint<1> submaps_flag;
                ss >> submaps_flag;
                os << submaps_flag;

                unsigned int submaps = 1;
                if (submaps_flag)
                {
                    Bit_uint<4> submaps_less1;

                    ss >> submaps_less1;
                    submaps = submaps_less1 + 1;
                    os << submaps_less1;
                }

                Bit_uint<1> square_polar_flag;
                ss >> square_polar_flag;
                os << square_polar_flag;

                if (square_polar_flag)
                {
                    Bit_uint<8> coupling_steps_less1;
                    ss >> coupling_steps_less1;
                    unsigned int coupling_steps = coupling_steps_less1 + 1;
                    os << coupling_steps_less1;

                    for (unsigned int j = 0; j < coupling_steps; j++)
                    {
                        Bit_uintv magnitude(ilog(_channels-1)), angle(ilog(_channels-1));

                        ss >> magnitude >> angle;
                        os << magnitude << angle;

                        if (angle == magnitude || magnitude >= _channels || angle >= _channels) throw Parse_error_str("invalid coupling");
                    }
                }

                // a rare reserved field not removed by Ak!
                Bit_uint<2> mapping_reserved;
                ss >> mapping_reserved;
                os << mapping_reserved;
                if (0 != mapping_reserved) throw Parse_error_str("mapping reserved field nonzero");

                if (submaps > 1)
                {
                    for (unsigned int j = 0; j < _channels; j++)
                    {
                        Bit_uint<4> mapping_mux;
                        ss >> mapping_mux;
                        os << mapping_mux;

                        if (mapping_mux >= submaps) throw Parse_error_str("mapping_mux >= submaps");
                    }
                }

                for (unsigned int j = 0; j < submaps; j++)
                {
                    // Another! Unused time domain transform configuration placeholder!
                    Bit_uint<8> time_config;
                    ss >> time_config;
                    os << time_config;

                    Bit_uint<8> floor_number;
                    ss >> floor_number;
                    os << floor_number;
                    if (floor_number >= floor_count) throw Parse_error_str("invalid floor mapping");

                    Bit_uint<8> residue_number;
                    ss >> residue_number;
                    os << residue_number;
                    if (residue_number >= residue_count) throw Parse_error_str("invalid residue mapping");
                }
            }

            // mode count
            Bit_uint<6> mode_count_less1;
            ss >> mode_count_less1;
            unsigned int mode_count = mode_count_less1 + 1;
            os << mode_count_less1;

            mode_blockflag = new bool [mode_count];
            mode_bits = ilog(mode_count-1);

            //cout << mode_count << " modes" << endl;

            for (unsigned int i = 0; i < mode_count; i++)
            {
                Bit_uint<1> block_flag;
                ss >> block_flag;
                os << block_flag;

                mode_blockflag[i] = (block_flag != 0);

                // only 0 valid for windowtype and transformtype
                Bit_uint<16> windowtype(0), transformtype(0);
                os << windowtype << transformtype;

                Bit_uint<8> mapping;
                ss >> mapping;
                os << mapping;
                if (mapping >= mapping_count) throw Parse_error_str("invalid mode mapping");
            }

            Bit_uint<1> framing(1);
            os << framing;

        } // _full_setup

        os.flush_page();

        if ((ss.get_total_bits_read()+7)/8 != setup_packet.size()) throw Parse_error_str("didn't read exactly setup packet");

        if (setup_packet.next_offset() != _data_offset + static_cast<long>(_first_audio_packet_offset)) throw Parse_error_str("first audio packet doesn't follow setup packet");

    }
}

void Wwise_RIFF_Vorbis::generate_ogg(ofstream& of)
{
    Bit_oggstream os(of);

    bool * mode_blockflag = NULL;
    int mode_bits = 0;
    bool prev_blockflag = false;

    if (_header_triad_present)
    {
        generate_ogg_header_with_triad(os);
    }
    else
    {
        generate_ogg_header(os, mode_blockflag, mode_bits);
    }

    // Audio pages
    {
        long offset = _data_offset + _first_audio_packet_offset;

        while (offset < _data_offset + _data_size)
        {
            uint32_t size, granule;
            long packet_header_size, packet_payload_offset, next_offset;

            if (_old_packet_headers)
            {
                Packet_8 audio_packet(_infile, offset, _little_endian);
                packet_header_size = audio_packet.header_size();
                size = audio_packet.size();
                packet_payload_offset = audio_packet.offset();
                granule = audio_packet.granule();
                next_offset = audio_packet.next_offset();
            }
            else
            {
                Packet audio_packet(_infile, offset, _little_endian, _no_granule);
                packet_header_size = audio_packet.header_size();
                size = audio_packet.size();
                packet_payload_offset = audio_packet.offset();
                granule = audio_packet.granule();
                next_offset = audio_packet.next_offset();
            }

            if (offset + packet_header_size > _data_offset + _data_size) {
                throw Parse_error_str("page header truncated");
            }

            offset = packet_payload_offset;

            _infile.seekg(offset);
            // HACK: don't know what to do here
            if (granule == UINT32_C(0xFFFFFFFF))
            {
                os.set_granule(1);
            }
            else
            {
                os.set_granule(granule);
            }

            // first byte
            if (_mod_packets)
            {
                // need to rebuild packet type and window info

                if (!mode_blockflag)
                {
                    throw Parse_error_str("didn't load mode_blockflag");
                }

                // OUT: 1 bit packet type (0 == audio)
                Bit_uint<1> packet_type(0);
                os << packet_type;

                Bit_uintv * mode_number_p = 0;
                Bit_uintv * remainder_p = 0;

                {
                    // collect mode number from first byte

                    Bit_stream ss(_infile);

                    // IN/OUT: N bit mode number (max 6 bits)
                    mode_number_p = new Bit_uintv(mode_bits);
                    ss >> *mode_number_p;
                    os << *mode_number_p;

                    // IN: remaining bits of first (input) byte
                    remainder_p = new Bit_uintv(8-mode_bits);
                    ss >> *remainder_p;
                }

                if (mode_blockflag[*mode_number_p])
                {
                    // long window, peek at next frame

                    _infile.seekg(next_offset);
                    bool next_blockflag = false;
                    if (next_offset + packet_header_size <= _data_offset + _data_size)
                    {

                        // mod_packets always goes with 6-byte headers
                        Packet audio_packet(_infile, next_offset, _little_endian, _no_granule);
                        uint32_t next_packet_size = audio_packet.size();
                        if (next_packet_size > 0)
                        {
                            _infile.seekg(audio_packet.offset());

                            Bit_stream ss(_infile);
                            Bit_uintv next_mode_number(mode_bits);

                            ss >> next_mode_number;

                            next_blockflag = mode_blockflag[next_mode_number];
                        }
                    }

                    // OUT: previous window type bit
                    Bit_uint<1> prev_window_type(prev_blockflag);
                    os << prev_window_type;

                    // OUT: next window type bit
                    Bit_uint<1> next_window_type(next_blockflag);
                    os << next_window_type;

                    // fix seek for rest of stream
                    _infile.seekg(offset + 1);
                }

                prev_blockflag = mode_blockflag[*mode_number_p];
                delete mode_number_p;

                // OUT: remaining bits of first (input) byte
                os << *remainder_p;
                delete remainder_p;
            }
            else
            {
                // nothing unusual for first byte
                int v = _infile.get();
                if (v < 0)
                {
                    throw Parse_error_str("file truncated");
                }
                Bit_uint<8> c(v);
                os << c;
            }

            // remainder of packet
            for (unsigned int i = 1; i < size; i++)
            {
                int v = _infile.get();
                if (v < 0)
                {
                    throw Parse_error_str("file truncated");
                }
                Bit_uint<8> c(v);
                os << c;
            }

            offset = next_offset;
            os.flush_page( false, (offset == _data_offset + _data_size) );
        }
        if (offset > _data_offset + _data_size) throw Parse_error_str("page truncated");
    }

    delete [] mode_blockflag;
}

void Wwise_RIFF_Vorbis::generate_ogg_header_with_triad(Bit_oggstream& os)
{
    // Header page triad
    {
        long offset = _data_offset + _setup_packet_offset;

        // copy information packet
        {
            Packet_8 information_packet(_infile, offset, _little_endian);
            uint32_t size = information_packet.size();

            if (information_packet.granule() != 0)
            {
                throw Parse_error_str("information packet granule != 0");
            }

            _infile.seekg(information_packet.offset());

            Bit_uint<8> c(_infile.get());
            if (1 != c)
            {
                throw Parse_error_str("wrong type for information packet");
            }

            os << c;

            for (unsigned int i = 1; i < size; i++)
            {
                c = _infile.get();
                os << c;
            }

            // identification packet on its own page
            os.flush_page();

            offset = information_packet.next_offset();
        }

        // copy comment packet 
        {
            Packet_8 comment_packet(_infile, offset, _little_endian);
            uint16_t size = comment_packet.size();

            if (comment_packet.granule() != 0)
            {
                throw Parse_error_str("comment packet granule != 0");
            }

            _infile.seekg(comment_packet.offset());

            Bit_uint<8> c(_infile.get());
            if (3 != c)
            {
                throw Parse_error_str("wrong type for comment packet");
            }

            os << c;

            for (unsigned int i = 1; i < size; i++)
            {
                c = _infile.get();
                os << c;
            }

            // identification packet on its own page
            os.flush_page();

            offset = comment_packet.next_offset();
        }

        // copy setup packet
        {
            Packet_8 setup_packet(_infile, offset, _little_endian);

            _infile.seekg(setup_packet.offset());
            if (setup_packet.granule() != 0) throw Parse_error_str("setup packet granule != 0");
            Bit_stream ss(_infile);

            Bit_uint<8> c;
            ss >> c;

            // type
            if (5 != c)
            {
                throw Parse_error_str("wrong type for setup packet");
            }
            os << c;

            // 'vorbis'
            for (unsigned int i = 0; i < 6; i++)
            {
                ss >> c;
                os << c;
            }

            // codebook count
            Bit_uint<8> codebook_count_less1;
            ss >> codebook_count_less1;
            unsigned int codebook_count = codebook_count_less1 + 1;
            os << codebook_count_less1;

            codebook_library cbl;

            // rebuild codebooks
            for (unsigned int i = 0; i < codebook_count; i++)
            {
                cbl.copy(ss, os);
            }

            while (ss.get_total_bits_read() < setup_packet.size()*8u)
            {
                Bit_uint<1> bitly;
                ss >> bitly;
                os << bitly;
            }

            os.flush_page();

            offset = setup_packet.next_offset();
        }

        if (offset != _data_offset + static_cast<long>(_first_audio_packet_offset)) throw Parse_error_str("first audio packet doesn't follow setup packet");

    }

}
