#ifndef _ERRORS_H
#define _ERRORS_H

#include <iostream>
#include <string>
using namespace std;

class Argument_error {
    string errmsg;
public:
    friend ostream& operator << (ostream& os, const Argument_error& ae) {
        os << "Argument error: " << ae.errmsg;
        return os;
    }
    
    explicit Argument_error(const char * str) : errmsg(str) {}
};

class File_open_error {
    string filename;
public:
    friend ostream& operator << (ostream& os, const File_open_error& fe) {
        os << "Error opening " << fe.filename;
        return os;
    }

    explicit File_open_error(const string& name) : filename(name) {}
};

class Parse_error {
public:
    virtual void print_self(ostream& os) const {
        os << "unspecified.";
    }

    friend ostream& operator << (ostream& os, const Parse_error& pe) {
        os << "Parse error: ";
        pe.print_self(os);
        return os;
    }
    virtual ~Parse_error() {}
};

class Parse_error_str : public Parse_error {
    string str;
public:
    virtual void print_self(ostream& os) const {
        os << str;
    }

    explicit Parse_error_str(string s) : str(s) {}
};

class Size_mismatch : public Parse_error {
    const unsigned long real_size, read_size;
public:
    virtual void print_self(ostream& os) const {
        os << "expected " << real_size << " bits, read " << read_size;
    }

    Size_mismatch(unsigned long real_s, unsigned long read_s) : real_size(real_s), read_size(read_s) {}
};

class Invalid_id : public Parse_error {
    const int id;
public:
    virtual void print_self(ostream& os) const {
        os << "invalid codebook id " << id << ", try --inline-codebooks";
    }

    explicit Invalid_id(int i) : id(i) {}
};


#endif
