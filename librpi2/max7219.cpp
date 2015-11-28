//////////////////////////////////////////////////////////////////////
// max7219.cpp -- MAX7219 Class Implementation
// Date: Mon Mar 16 21:02:17 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "max7219.hpp"
#include "gpio.hpp"
#include "piutils.hpp"

#define Max(a,b) ((a)>=(b)?(a):(b))

MAX7219::MAX7219(int clk,int din,int load) {

    pin_clk = clk;          // GPIO pin for CLK
    pin_din = din;          // GPIO pin for DIN
    pin_load = load;        // GPIO pin for LOAD

    decodes = 0b000000000;  // Assume no decodes
    duty_cfg = 15;          // 50% brightness
    N = 8;                  // 8 rows (digits)

    errcode = gpio.configure(pin_clk,GPIO::Output);
    if ( errcode )
        return;             // GPIO failed to open

    gpio.configure(pin_din,GPIO::Output);
    gpio.configure(pin_load,GPIO::Output);
    gpio.write(pin_clk,0);
    gpio.write(pin_din,1);
    gpio.write(pin_load,0);

    tCH = 50;               // ns
    tCL = 50;
    tDS = 25;
    tLDCK = 50;
    tCSW = 50;
}

int
MAX7219::wrbit(int b,int last) {

    if ( errcode )
        return errcode;     // GPIO open failure

    gpio.write(pin_din,b);  // Set state of DIN
    nswait(tDS+1);          // Wait min setup time
    gpio.write(pin_clk,1);  // Set clock high
    if ( last )
        gpio.write(pin_load,1);
    nswait(Max(tDS,tCH));   // DIN setup & CLK high
    gpio.write(pin_clk,0);  // set CLK low
    nswait(tCL);            // wait out CLK low
    return 0;
}

int
MAX7219::write(unsigned cmd16) {
    
    if ( errcode )
        return errcode;     // GPIO open failure

    gpio.write(pin_load,0);
    
    for ( unsigned bx=16; bx-- > 0; )
        wrbit((cmd16 >> bx) & 1,!bx);
    
    nswait(Max(tCSW,tLDCK));
    return 0;
}

int
MAX7219::nop() {

    if ( errcode )
        return errcode;     // GPIO open failure

    write(0x0000);
    return 0;
}

int
MAX7219::shutdown() {

    if ( errcode )
        return errcode;     // GPIO open failure

    write(0x0C00);
    return 0;
}

int
MAX7219::enable() {

    if ( errcode )
        return errcode;     // GPIO open failure

    write(0x0CFF);
    return 0;
}

int
MAX7219::test(bool on) {

    if ( errcode )
        return errcode;     // GPIO open failure

    write(0x0F00 | (on ? 0xFF : 0x00));
    return 0;
}

int
MAX7219::config_decode(int digit,bool decode) {

    if ( errcode )
        return errcode;     // GPIO open failure
    else if ( digit < 0 || digit > 7 )
        return EINVAL;      // digit is out of range

    if ( decode )
        decodes |= (1 << digit);    // BCD decode
    else
        decodes &= ~(1 << digit);   // No BCD

    write(0x0900 | (decodes & 0xFF));
    return 0;
}

int
MAX7219::config_digits(int n_digits) {      // 1 - 8

    if ( errcode )
        return errcode;     // GPIO open failure
    else if ( n_digits < 1 || n_digits > 8 )
        return EINVAL;      // n_digits out of range

    unsigned code = unsigned(n_digits) - 1; // 0 - 7

    write(0x0B00 | code);
    return 0;
}

int
MAX7219::config_intensity(int n) {  // 0-15

    if ( errcode )
        return errcode;     // GPIO open failure
    else if ( n < 0 || n > 15 )
        return EINVAL;      // n out of range

    unsigned intensity = unsigned(n);

    assert(n >= 0 && n <= 15);
    write(0x0A00 | intensity);
    return 0;
}

int
MAX7219::data(int digit,int data) {

    if ( errcode )
        return errcode;     // GPIO open failure
    if ( digit < 0 || digit > 7 )
        return EINVAL;      // Digit out of range

    unsigned rdata = unsigned(data) & 0x0FF;
    unsigned dig = unsigned(digit) & 0x07;

    write(((dig+1) << 8) | rdata);
    return 0;
}

// End max7219.cpp
