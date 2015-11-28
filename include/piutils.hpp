//////////////////////////////////////////////////////////////////////
// piutils.hpp -- Raspberry Pi Utility Library
// Date: Sat Mar 14 23:17:58 2015   (C) Warren Gay ve3wwg
///////////////////////////////////////////////////////////////////////

#ifndef PIUTILS_HPP
#define PIUTILS_HPP

#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <string>

enum Architecture {
    Unknown,
    ARMv6,
    ARMv7       // Pi 2
};

void nswait(unsigned long ns); // Nanoseconds
void uswait(unsigned long us); // Microseconds
void mswait(unsigned long ms); // Milliseconds

uint32_t sys_page_size();
bool model_and_revision(std::string& model,Architecture& arch,uint32_t& revision,std::string& serial);

#endif // PIUTILS_HPP

// End piutils.hpp
