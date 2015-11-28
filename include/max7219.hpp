//////////////////////////////////////////////////////////////////////
// max7219.hpp -- The MAX7219 Class
// Date: Mon Mar 16 20:55:11 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef MAX7219_HPP
#define MAX7219_HPP


#include "gpio.hpp"

class MAX7219 {
    int         pin_clk;    // GPIO pin used for CLK
    int         pin_din;    // GPIO pin used for DIN
    int         pin_load;   // GPIO pin used for LOAD

    unsigned    decodes;    // 1=decode, 0=no decode (bits 0-7)
    unsigned    duty_cfg;   // For 7219: 0-31
    unsigned    N;          // # of segments driven	

    unsigned    tCH;        // CLK pulse width high
    unsigned    tCL;        // CLK pulse width low
    unsigned    tDS;        // DIN setup time
    unsigned    tLDCK;      // Load rising edge to next CLK
    unsigned    tCSW;       // Min LOAD pulse high

    int         errcode;    // errno if GPIO failed

protected:

    GPIO        gpio;       // GPIO access

    int wrbit(int b,int last=0);
    int write(unsigned cmd16);

public:

    MAX7219(int clk,int din,int load);

    int nop();                              // No op
    int shutdown();                         // Shut device down
    int test(bool on);                      // Display test mode
    int enable();                           // Enable normal operation

    int config_decode(int digit,bool decode); // Decode mode per digit

    int config_digits(int n_digits);        // Configure 1-8 digits
    int config_intensity(int n);            // Configure intensity 0-15

    int data(int digit,int data);
};


#endif // MAX7219_HPP

// End max7219.hpp

