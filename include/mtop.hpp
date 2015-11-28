//////////////////////////////////////////////////////////////////////
// mtop.hpp -- Matrix "Top" For Raspberry Pi 2
// Date: Sat Feb 14 21:12:33 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef MTOP_HPP
#define MTOP_HPP

#include <time.h>
#include <vector>

class MTop {
    typedef unsigned long long cpums_t;

    timespec                tv_before;  // Time of first sample
    std::vector<cpums_t>    before;     // Snapshot of times before
    timespec                tv_after;   // Time of 2nd sample
    std::vector<cpums_t>    after;      // Current snapshots
    unsigned                samples;    // # of samples taken

    unsigned long           mem_total;  // Total system memory kb
    unsigned long           mem_free;   // Free system memory kb
    unsigned long           swap_total; // Total swap kb
    unsigned long           swap_free;  // Free swap kb

protected:

    double last_total_cpu_pct;          // Last total CPU sample in %

    int take_sample(timespec& tv,std::vector<cpums_t>& cpuvec);

public:	MTop();

    int sample(std::vector<double>& cpus);  // Return CPU %s
    double total_cpu_pct() const;           // Return last total CPU %
    double memory_pct();                    // Return memory used (%)
    double swap_pct();                      // Return swap used (%)
};

class Diskstat {
    typedef unsigned long long io_ms_t;

    bool                    started;
    double                  io0, io1;
    double                  t0, t1;
    double                  max_ms;

protected:
    io_ms_t get_io();

public:
    Diskstat();

    double pct_io();
};

#endif // MTOP_HPP

// End mtop.hpp
