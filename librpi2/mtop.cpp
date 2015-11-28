/////////////////////////////////////////////////////////////
// mtop.cpp -- Matrix "Top" for Raspberry Pi 2
// Date: Sat Feb 14 21:12:54 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
/////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "mtop.hpp"
#include "matrix.hpp"

/////////////////////////////////////////////////////////////
// Return time as a double float (seconds)
/////////////////////////////////////////////////////////////

static double
dtime(timespec& tv) {
    double t = tv.tv_sec;

    t += double(tv.tv_nsec) / 1000000000.0;
    return t;
}

/////////////////////////////////////////////////////////////
// Get time, and returned it as a double
/////////////////////////////////////////////////////////////

static double
get_dtime() {
    timespec tv;
	
    int rc = clock_gettime(CLOCK_MONOTONIC,&tv);
    assert(!rc);
    (void)rc;
    return dtime(tv);
}

/////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////

MTop::MTop() {
    tv_before.tv_sec = 0;
    tv_after.tv_sec = 0;
    samples = 0;
    last_total_cpu_pct = 0.0;
}

/////////////////////////////////////////////////////////////
// /proc/stat:
//
// cpu  39148 1509 20021 18396412 14338 4 43 0 0 0
// cpu0 5947 97 1582 2299728 1962 0 15 0 0 0
// cpu1 6025 69 1617 2299513 1621 0 4 0 0 0
// cpu2 4131 84 1980 2301519 1314 0 2 0 0 0
// cpu3 4447 410 1824 2300495 1654 0 11 0 0 0
// intr 10767213 743493 3 0 0 0 0 0 0 1 0 0 0 4 0 0 0 0 ...
// ctxt 4568428
// btime 1423944848
// processes 114960
// procs_running 2
// ...
/////////////////////////////////////////////////////////////

int
MTop::take_sample(timespec& tv,std::vector<cpums_t>& cpuvec) {
    FILE *fp;		// /proc/stat or /proc/meminfo
    int rc, n;

    /////////////////////////////////////////////////////////
    // Take CPU utilization sample
    /////////////////////////////////////////////////////////

    cpuvec.clear();

    fp = fopen("/proc/stat","r");
    if ( !fp )
        return -1;

    rc = clock_gettime(CLOCK_MONOTONIC,&tv);
    assert(!rc);

    char rbuf[1024], cpu[64], ignored[64];
    double user,nice,system,idle;

    while ( fgets(rbuf,sizeof rbuf,fp) ) {
        if ( strncmp(rbuf,"cpu",3) != 0 )
            break;      // Quit after cpu readings
        n = sscanf(rbuf,"%s %lf %lf %lf %lf",
            cpu,&user,&nice,&system,&idle);
        if ( n != 5 )
            break;      // Something fishy 
        cpuvec.push_back(user+system); // ms
    }

    fclose(fp);

    /////////////////////////////////////////////////////////
    // Get memory & swap info
    /////////////////////////////////////////////////////////

    fp = fopen("/proc/meminfo","r");

    while ( fgets(rbuf,sizeof rbuf,fp) ) {
        if ( !strncmp(rbuf,"MemTotal:",9) ) {
            n = sscanf(rbuf,"%s %lu",ignored,&mem_total);
            assert(n == 2);
        } else if ( !strncmp(rbuf,"MemFree:",8) ) {
            n = sscanf(rbuf,"%s %lu",ignored,&mem_free);
            assert(n == 2);
        } else if ( !strncmp(rbuf,"SwapTotal:",10) ) {
            n = sscanf(rbuf,"%s %lu",ignored,&swap_total);
            assert(n == 2);
        } else if ( !strncmp(rbuf,"SwapFree:",9) ) {
            n = sscanf(rbuf,"%s %lu",ignored,&swap_free);
            assert(n == 2);
            break;
        }
    }	

    fclose(fp);

    return 0;	
}

/////////////////////////////////////////////////////////////
// Take a sample of CPU activity:
//
// ARGUMENTS:
//  cpus    std::vector<double> of returned cpu percentages
//
// RETURNS:
//  -1  An error occurred opening /proc/stat (check errno)
//   0  A first sample was taken (no data returned)
//  >0  An nth sample was taken (data was returned)
/////////////////////////////////////////////////////////////

int
MTop::sample(std::vector<double>& cpus) {
    int rc;

    cpus.clear();           // Empty vector

    if ( samples >= 1 ) {
        before.clear();
        tv_before = tv_after;
        before = after;
        after.clear();
    }

    rc = take_sample(tv_after,after);
    if ( ++samples <= 1 )
        return rc;

    assert(before.size() == after.size());
		
    double n_cpus = before.size() - 1;

    for ( size_t x=0; x<before.size(); ++x ) {
        double used_ms = after[x] - before[x];
        double time_secs = dtime(tv_after) - dtime(tv_before);
        double pct;

        if ( x == 0 ) {
            // All cpus combined
            last_total_cpu_pct = pct = used_ms / ( time_secs * n_cpus );
        } else	{
            // Individual cpus
            pct = used_ms / time_secs;
        }
        if ( pct > 100.0 )
            pct = 100.0;
            cpus.push_back(pct);
    }

    return samples - 1; // 1..
}

//////////////////////////////////////////////////////////////////////
// Return the last total CPU % computed
//////////////////////////////////////////////////////////////////////

double
MTop::total_cpu_pct() const {
    return last_total_cpu_pct;
}

/////////////////////////////////////////////////////////////
// Return system memory used as %
/////////////////////////////////////////////////////////////

double
MTop::memory_pct() {
    return double(mem_total - mem_free)
         / double(mem_total) * 100.0;
}

/////////////////////////////////////////////////////////////
// Return system swap used as %
/////////////////////////////////////////////////////////////

double
MTop::swap_pct() {
    return double(swap_total - swap_free)
        / double(swap_total) * 100.0;
}

/////////////////////////////////////////////////////////////
// Measure relative diskstat activity
/////////////////////////////////////////////////////////////

Diskstat::Diskstat() {

    started = true;
    
    t0 = get_dtime();
    io0 = get_io();
    max_ms = 0.0;
}

/////////////////////////////////////////////////////////////
// Return the ms total for all devices (but not partitions of)
/////////////////////////////////////////////////////////////

Diskstat::io_ms_t
Diskstat::get_io() {
    FILE *fp = fopen("/proc/diskstats","r");
    char buf[2048];
    io_ms_t io_total = 0;
    unsigned minor;
    long io_time;

    assert(fp);
    while ( fgets(buf,sizeof buf,fp) ) {
        // Total field 13 "TIme spent doing I/Os (ms)
        sscanf(buf,"%*s %u %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %ld\n",&minor,&io_time);
        if ( minor == 0 )           // Count only the main device
            io_total += io_time;    // .. not the partitions
    }

    fclose(fp);

    return io_total;
}

/////////////////////////////////////////////////////////////
// Return relative activity in terms of percent
/////////////////////////////////////////////////////////////

double
Diskstat::pct_io() {

    if ( !started ) {
        t0 = t1;
        io0 = io1;
    } else {
    	started = false;
    }

    t1 = get_dtime();
    io1 = get_io();
    
    double avg_ms = ( io1 - io0 ) / (t1 - t0);

    if ( avg_ms > max_ms )
        max_ms = avg_ms;

    if ( max_ms <= 0.0 )
        return 0.0;
    double pct = avg_ms / max_ms * 100.0;

    return pct;
}

// End mtop.cpp
