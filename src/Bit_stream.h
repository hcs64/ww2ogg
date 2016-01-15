#ifndef _BIT_STREAM_H
#define _BIT_STREAM_H

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <iostream>
#include <limits>
#include <stdint.h>

#include "errors.h"
#include "crc.h"

// host-endian-neutral integer reading
namespace {
    uint32_t read_32_le(unsigned char b[4])
    {
        uint32_t v = 0;
        for (int i = 3; i >= 0; i--)
        {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint32_t read_32_le(std::istream &is)
    {
        char b[4];
        is.read(b, 4);

        return read_32_le(reinterpret_cast<unsigned char *>(b));
    }

    void write_32_le(unsigned char b[4], uint32_t v)
    {
        for (int i = 0; i < 4; i++)
        {
            b[i] = v & 0xFF;
            v >>= 8;
        }
    }

    void write_32_le(std::ostream &os, uint32_t v)
    {
        char b[4];

        write_32_le(reinterpret_cast<unsigned char *>(b), v);

        os.write(b, 4);
    }

    uint16_t read_16_le(unsigned char b[2])
    {
        uint16_t v = 0;
        for (int i = 1; i >= 0; i--)
        {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint16_t read_16_le(std::istream &is)
    {
        char b[2];
        is.read(b, 2);

        return read_16_le(reinterpret_cast<unsigned char *>(b));
    }

    void write_16_le(unsigned char b[2], uint16_t v)
    {
        for (int i = 0; i < 2; i++)
        {
            b[i] = v & 0xFF;
            v >>= 8;
        }
    }

    void write_16_le(std::ostream &os, uint16_t v)
    {
        char b[2];

        write_16_le(reinterpret_cast<unsigned char *>(b), v);

        os.write(b, 2);
    }

    uint32_t read_32_be(unsigned char b[4])
    {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++)
        {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint32_t read_32_be(std::istream &is)
    {
        char b[4];
        is.read(b, 4);

        return read_32_be(reinterpret_cast<unsigned char *>(b));
    }

    void write_32_be(unsigned char b[4], uint32_t v)
    {
        for (int i = 3; i >= 0; i--)
        {
            b[i] = v & 0xFF;
            v >>= 8;
        }
    }

    void write_32_be(std::ostream &os, uint32_t v)
    {
        char b[4];

        write_32_be(reinterpret_cast<unsigned char *>(b), v);

        os.write(b, 4);
    }

    uint16_t read_16_be(unsigned char b[2])
    {
        uint16_t v = 0;
        for (int i = 0; i < 2; i++)
        {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint16_t read_16_be(std::istream &is)
    {
        char b[2];
        is.read(b, 2);

        return read_16_be(reinterpret_cast<unsigned char *>(b));
    }

    void write_16_be(unsigned char b[2], uint16_t v)
    {
        for (int i = 1; i >= 0; i--)
        {
            b[i] = v & 0xFF;
            v >>= 8;
        }
    }

    void write_16_be(std::ostream &os, uint16_t v)
    {
        char b[2];

        write_16_be(reinterpret_cast<unsigned char *>(b), v);

        os.write(b, 2);
    }

}

// using an istream, pull off individual bits with get_bit (LSB first)
class Bit_stream {
    std::istream& is;

    unsigned char bit_buffer;
    unsigned int bits_left;
    unsigned long total_bits_read;

public:
    class Weird_char_size {};
    class Out_of_bits {};

    Bit_stream(std::istream& _is) : is(_is), bit_buffer(0), bits_left(0), total_bits_read(0) {
        if ( std::numeric_limits<unsigned char>::digits != 8)
            throw Weird_char_size();
    }
    bool get_bit() {
        if (bits_left == 0) {

            int c = is.get();
            if (c == EOF) throw Out_of_bits();
            bit_buffer = c;
            bits_left = 8;

        }
        total_bits_read++;
        bits_left --;
        return ( ( bit_buffer & ( 0x80 >> bits_left ) ) != 0);
    }

    unsigned long get_total_bits_read(void) const
    {
        return total_bits_read;
    }
};

class Bit_oggstream {
    std::ostream& os;

    unsigned char bit_buffer;
    unsigned int bits_stored;

    enum {header_bytes = 27, max_segments = 255, segment_size = 255};

    unsigned int payload_bytes;
    bool first, continued;
    unsigned char page_buffer[header_bytes + max_segments + segment_size * max_segments];
    uint32_t granule;
    uint32_t seqno;

public:
    class Weird_char_size {};

    Bit_oggstream(std::ostream& _os) :
        os(_os), bit_buffer(0), bits_stored(0), payload_bytes(0), first(true), continued(false), granule(0), seqno(0) {
        if ( std::numeric_limits<unsigned char>::digits != 8)
            throw Weird_char_size();
        }

    void put_bit(bool bit) {
        if (bit)
        bit_buffer |= 1<<bits_stored;

        bits_stored ++;
        if (bits_stored == 8) {
            flush_bits();
        }
    }

    void set_granule(uint32_t g) {
        granule = g;
    }

    void flush_bits(void) {
        if (bits_stored != 0) {
            if (payload_bytes == segment_size * max_segments)
            {
                throw Parse_error_str("ran out of space in an Ogg packet");
                flush_page(true);
            }

            page_buffer[header_bytes + max_segments + payload_bytes] = bit_buffer;
            payload_bytes ++;

            bits_stored = 0;
            bit_buffer = 0;
        }
    }

    void flush_page(bool next_continued=false, bool last=false) {
        if (payload_bytes != segment_size * max_segments)
        {
            flush_bits();
        }

        if (payload_bytes != 0)
        {
            unsigned int segments = (payload_bytes+segment_size)/segment_size;  // intentionally round up
            if (segments == max_segments+1) segments = max_segments; // at max eschews the final 0

            // move payload back
            for (unsigned int i = 0; i < payload_bytes; i++)
            {
                page_buffer[header_bytes + segments + i] = page_buffer[header_bytes + max_segments + i];
            }

            page_buffer[0] = 'O';
            page_buffer[1] = 'g';
            page_buffer[2] = 'g';
            page_buffer[3] = 'S';
            page_buffer[4] = 0; // stream_structure_version
            page_buffer[5] = (continued?1:0) | (first?2:0) | (last?4:0); // header_type_flag
            write_32_le(&page_buffer[6], granule);  // granule low bits
            write_32_le(&page_buffer[10], 0);       // granule high bits
            if (granule == UINT32_C(0xFFFFFFFF))
                write_32_le(&page_buffer[10], UINT32_C(0xFFFFFFFF));
            write_32_le(&page_buffer[14], 1);       // stream serial number
            write_32_le(&page_buffer[18], seqno);   // page sequence number
            write_32_le(&page_buffer[22], 0);       // checksum (0 for now)
            page_buffer[26] = segments;             // segment count

            // lacing values
            for (unsigned int i = 0, bytes_left = payload_bytes; i < segments; i++)
            {
                if (bytes_left >= segment_size)
                {
                    bytes_left -= segment_size;
                    page_buffer[27 + i] = segment_size;
                }
                else
                {
                    page_buffer[27 + i] = bytes_left;
                }
            }

            // checksum
            write_32_le(&page_buffer[22],
                    checksum(page_buffer, header_bytes + segments + payload_bytes)
                    );

            // output to ostream
            for (unsigned int i = 0; i < header_bytes + segments + payload_bytes; i++)
            {
                os.put(page_buffer[i]);
            }

            seqno++;
            first = false;
            continued = next_continued;
            payload_bytes = 0;
        }
    }

    ~Bit_oggstream() {
        flush_page();
    }
};

// integer of a certain number of bits, to allow reading just that many
// bits from the Bit_stream
template <unsigned int BIT_SIZE>
class Bit_uint {
    unsigned int total;
public:
    class Too_many_bits {};
    class Int_too_big {};

    Bit_uint() : total(0) {
        if (BIT_SIZE > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw Too_many_bits();
    }

    explicit Bit_uint(unsigned int v) : total(v) {
        if (BIT_SIZE > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw Too_many_bits();
        if ((v >> (BIT_SIZE-1U)) > 1U)
            throw Int_too_big();
    }

    Bit_uint& operator = (unsigned int v) {
        if ((v >> (BIT_SIZE-1U)) > 1U)
            throw Int_too_big();
        total = v;
        return *this;
    }

    operator unsigned int() const { return total; }

    friend Bit_stream& operator >> (Bit_stream& bstream, Bit_uint& bui) {
        bui.total = 0;
        for ( unsigned int i = 0; i < BIT_SIZE; i++) {
            if ( bstream.get_bit() ) bui.total |= (1U << i);
        }
        return bstream;
    }

    friend Bit_oggstream& operator << (Bit_oggstream& bstream, const Bit_uint& bui) {
        for ( unsigned int i = 0; i < BIT_SIZE; i++) {
            bstream.put_bit((bui.total & (1U << i)) != 0);
        }
        return bstream;
    }
};

// integer of a run-time specified number of bits
// bits from the Bit_stream
class Bit_uintv {
    unsigned int size;
    unsigned int total;
public:
    class Too_many_bits {};
    class Int_too_big {};

    explicit Bit_uintv(unsigned int s) : size(s), total(0) {
        if (s > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw Too_many_bits();
    }

    Bit_uintv(unsigned int s, unsigned int v) : size(s), total(v) {
        if (size > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw Too_many_bits();
        if ((v >> (size-1U)) > 1U)
            throw Int_too_big();
    }

    Bit_uintv& operator = (unsigned int v) {
        if ((v >> (size-1U)) > 1U)
            throw Int_too_big();
        total = v;
        return *this;
    }

    operator unsigned int() const { return total; }

    friend Bit_stream& operator >> (Bit_stream& bstream, Bit_uintv& bui) {
        bui.total = 0;
        for ( unsigned int i = 0; i < bui.size; i++) {
            if ( bstream.get_bit() ) bui.total |= (1U << i);
        }
        return bstream;
    }

    friend Bit_oggstream& operator << (Bit_oggstream& bstream, const Bit_uintv& bui) {
        for ( unsigned int i = 0; i < bui.size; i++) {
            bstream.put_bit((bui.total & (1U << i)) != 0);
        }
        return bstream;
    }
};

class array_streambuf : public std::streambuf
{
    // Intentionally undefined
    array_streambuf& operator=(const array_streambuf& rhs);
    array_streambuf(const array_streambuf &rhs);

    char * arr;
        
public:
    array_streambuf(const char * a, int l) : arr(0)
    {
        arr = new char [l];
        for (int i = 0; i < l; i++)
            arr[i] = a[i];
        setg(arr, arr, arr+l);
    }
    ~array_streambuf()
    {
        delete [] arr;
    }
};

#endif // _BIT_STREAM_H
