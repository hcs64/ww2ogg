#define __STDC_CONSTANT_MACROS
#include <iostream>
#include <fstream>
#include <cstring>
#include "wwriff.h"
#include "stdint.h"
#include "errors.h"
#ifdef __unix__
#include <unistd.h>
#define _isatty isatty
#define _fileno fileno
#endif

using namespace std;

class ww2ogg_options
{
    string in_filename;
    string out_filename;
    string codebooks_filename;
    bool inline_codebooks;
    bool full_setup;
    ForcePacketFormat force_packet_format;
    bool quiet;
    bool to_stdout;
public:
    ww2ogg_options(void) : in_filename(""),
                           out_filename(""),
                           codebooks_filename("packed_codebooks.bin"),
                           inline_codebooks(false),
                           full_setup(false),
                           force_packet_format(kNoForcePacketFormat),
                           quiet(false),
                           to_stdout(false)
      {}
    void parse_args(int argc, char **argv);
    const string& get_in_filename(void) const {return in_filename;}
    const string& get_out_filename(void) const {return out_filename;}
    const string& get_codebooks_filename(void) const {return codebooks_filename;}
    bool get_inline_codebooks(void) const {return inline_codebooks;}
    bool get_full_setup(void) const {return full_setup;}
    ForcePacketFormat get_force_packet_format(void) const {return force_packet_format;}
    bool get_quiet(void) const {return quiet;}
    bool get_to_stdout(void) const {return to_stdout;}
};

void header(void)
{
    cout << "Audiokinetic Wwise RIFF/RIFX Vorbis to Ogg Vorbis converter " VERSION " by hcs" << endl << endl;
}

void usage(void)
{
    cout << endl;
    header();
    cout << "usage: ww2ogg input.wav [-o output.ogg] [--inline-codebooks] [--full-setup]" << endl <<
            "                        [--mod-packets | --no-mod-packets]" << endl <<
            "                        [--pcb packed_codebooks.bin] [--quiet]" << endl <<
            endl <<
            "Use \"-o -\" to send the ogg file to stdout.  This will also" << endl <<
            "imply \"--quiet\"." << endl << endl;
}

int main(int argc, char **argv)
{

    ww2ogg_options opt;

    try
    {
        opt.parse_args(argc, argv);
    }
    catch (const Argument_error& ae)
    {
        cout << ae << endl;

        usage();
        return 1;
    }

    if (!opt.get_quiet())
    {
        header();
    }

    try
    {
        if (!opt.get_quiet())
        {
            cout << "Input: " << opt.get_in_filename() << endl;
        }
        Wwise_RIFF_Vorbis ww(opt.get_in_filename(),
                opt.get_codebooks_filename(),
                opt.get_inline_codebooks(),
                opt.get_full_setup(),
                opt.get_force_packet_format()
                );

        if (!opt.get_quiet())
        {
            ww.print_info();
        }

        if (opt.get_to_stdout())
        {
#if defined(_WIN32) || defined(_WIN64)
            _setmode(_fileno(stdout),  _O_BINARY);
#endif
            ww.generate_ogg(cout);
        }
        else
        {
            if (!opt.get_quiet())
            {
                cout << "Output: " << opt.get_out_filename() << endl;
            }
            ofstream of(opt.get_out_filename().c_str(), ios::binary);
            if (!of) throw File_open_error(opt.get_out_filename());
            ww.generate_ogg(of);
        }

        if (!opt.get_quiet())
        {
            cout << "Done!" << endl << endl;
        }
    }
    catch (const File_open_error& fe)
    {
        cerr << fe << endl;
        return 1;
    }
    catch (const Parse_error& pe)
    {
        cerr << pe << endl;
        return 1;
    }

    return 0;
}

void ww2ogg_options::parse_args(int argc, char ** argv)
{
    bool set_input = false, set_output = false;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-o"))
        {
            // switch for output file name
            if (i+1 >= argc)
            {
                throw Argument_error("-o needs an option");
            }

            if (set_output or to_stdout)
            {
                throw Argument_error("only one output file at a time");
            }

            out_filename = argv[++i];
            if (out_filename == "-")
            {
                if (_isatty(_fileno(stdout)))
                {
                    throw Argument_error("STDOUT output is not supported when on a TTY");
                }
                to_stdout = true;
                quiet = true;
            }
            set_output = true;
        }
        else if (!strcmp(argv[i], "--inline-codebooks"))
        {
            // switch for inline codebooks
            inline_codebooks = true;
        }
        else if (!strcmp(argv[i], "--full-setup"))
        {
            // early version with setup almost entirely intact
            full_setup = true;
            inline_codebooks = true;
        }
        else if (!strcmp(argv[i], "--mod-packets") || !strcmp(argv[i], "--no-mod-packets"))
        {
            if (force_packet_format != kNoForcePacketFormat)
            {
                throw Argument_error("only one of --mod-packets or --no-mod-packets is allowed");
            }

            if (!strcmp(argv[i], "--mod-packets"))
            {
              force_packet_format = kForceModPackets;
            }
            else
            {
              force_packet_format = kForceNoModPackets;
            }
        }
        else if (!strcmp(argv[i], "--pcb"))
        {
            // override default packed codebooks file
            if (i+1 >= argc)
            {
                throw Argument_error("--pcb needs an option");
            }

            codebooks_filename = argv[++i];
        }
        else if (!strcmp(argv[i], "--quiet"))
        {
            // don't output any status messages to stdout (will be
            // forced active if our output is stdout)
            quiet = true;
        }
        else
        {
            // assume anything else is an input file name
            if (set_input)
            {
                throw Argument_error("only one input file at a time");
            }

            in_filename = argv[i];
            set_input = true;
        }
    }

    if (!set_input)
    {
        throw Argument_error("input name not specified");
    }

    if (!set_output)
    {
        size_t found = in_filename.find_last_of('.');

        out_filename = in_filename.substr(0, found);
        out_filename.append(".ogg");

        // TODO: should be case insensitive for Windows
        if (out_filename == in_filename)
        {
            out_filename.append("_conv.ogg");
        }
    }
}
