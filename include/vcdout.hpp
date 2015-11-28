//////////////////////////////////////////////////////////////////////
// vcdout.hpp -- Value Change Dump
// Date: Thu Apr 30 18:59:27 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <time.h>

#include <string>
#include <map>

class VCD_Out {
    std::string pathname;
    time_t      tdate;
    FILE        *vcdf;
    bool        defns;
    unsigned    last_time;
    unsigned    time;

    struct s_defn {
        char        chref;
        std::string name;
        int         state;
    };

    std::map<int,s_defn> wires;
    void write_defns();

public:
    VCD_Out();
    ~VCD_Out();

    inline const char *get_pathname() {
        return pathname.c_str();
    }

    bool open(const char *path,double n,const char *units,const char *vers);
    void close();
    void define_binary(int ref,const char *name);

    void set_time(unsigned t);
    void set_value(int ref,bool value);
};

// End vcdout.hpp
