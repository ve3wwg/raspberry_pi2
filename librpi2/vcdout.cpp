//////////////////////////////////////////////////////////////////////
// vcdout.cpp -- VCD Data Output
// Date: Fri Apr 24 19:00:22 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "vcdout.hpp"

VCD_Out::VCD_Out() {
    vcdf = 0;
    defns = false;
}

VCD_Out::~VCD_Out() {
    close();
}

bool
VCD_Out::open(const char *path,double n,const char *units,const char *vers) {

    if ( vcdf )
        close();

    vcdf = fopen(path,"w");
    pathname = path;
    if ( !vcdf )
        return false;

    tdate = ::time(0);
    {
        struct tm tc;
        char tbuf[256];
    
        localtime_r(&tdate,&tc);
        strftime(tbuf,sizeof tbuf,"%Y-%m-%d %H:%M:%S",&tc);

        fprintf(vcdf,"$date %s $end\n",tbuf);
    }

    fprintf(vcdf,"$version %s $end\n",vers ? vers : "");
    fprintf(vcdf,"$timescale %g %s $end\n",n,units);
    fputs("$scope module top $end\n",vcdf);
    fflush(vcdf);

    defns = false;
    last_time = ~0u;
    time = 0;

    return true;
}

void
VCD_Out::close() {
    if ( vcdf ) {
        fclose(vcdf);
        vcdf = 0;
    }
    pathname.clear();
}

void
VCD_Out::define_binary(int ref,const char *name) {
    size_t n = wires.size();
    s_defn defn;

    if ( n < 26 )
        defn.chref = 'A' + n;
    else
        defn.chref = 'a' + n - 26;
    defn.name = name;
    defn.state = -1;
    wires[ref] = defn;
}

void
VCD_Out::write_defns() {

    assert(vcdf);
    for ( auto it = wires.cbegin(); it != wires.cend(); ++it ) {
        const s_defn& defn = it->second;

        fprintf(vcdf,"$var wire 1 %c %s $end\n",
            defn.chref,defn.name.c_str());
    }    
    fflush(vcdf);
    defns = true;
}

void
VCD_Out::set_time(unsigned t) {

    if ( !defns )
        write_defns();

    time = t;
}

void
VCD_Out::set_value(int ref,bool value) {

    if ( !defns ) {
        write_defns();
        set_time(0);
    }

    auto it = wires.find(ref);
    assert(it != wires.end());

    s_defn& defn = it->second;
    if ( defn.state != int(value) ) {
        if ( time != last_time ) {
            fprintf(vcdf,"#%u\n",time);
            last_time = time;
        }

        fprintf(vcdf,"%d%c\n",
            value ? 1 : 0,
            defn.chref);
        defn.state = int(value);
    }
}

// End vcdout.cpp
