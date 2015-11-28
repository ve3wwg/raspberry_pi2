//////////////////////////////////////////////////////////////////////
// matrix.cpp -- Matrix with MAX7219 Driver
// Date: Tue Mar 17 20:36:02 2015  (C) Warren W. Gay VE3WWG 
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
#include <assert.h>

#include "matrix.hpp"
#include "piutils.hpp"

//////////////////////////////////////////////////////////////////////
// Constructor: Initialize the GPIO and pins
//////////////////////////////////////////////////////////////////////

Matrix::Matrix(int clk,int din,int load)
: MAX7219(clk,din,load) {

    uswait(250);    
    enable();                       // In case it was shutdown
    uswait(250);    

    errcode = config_intensity(0);  // Lowest intensity
    if ( errcode )
        return;                     // GPIO open failed	

    config_digits(8);               // 8 digits
    config_intensity(3);            // Brighter

    for ( int dx=0; dx<=7; ++dx ) {
        config_decode(dx,0);        // No BCD decode
        display(dx,0);              // Blank display
    }
}

//////////////////////////////////////////////////////////////////////
// Destructor
//////////////////////////////////////////////////////////////////////

Matrix::~Matrix() {
    if ( meter_gpio > 0 ) {
        gpio.stop_clock(meter_gpio);            // Stop GPIO clock
        gpio.pwm_enable(meter_gpio,false);      // Disable
        gpio.configure(meter_gpio,GPIO::Input); // Revert to input
    }
}

//////////////////////////////////////////////////////////////////////
// Configure meter GPIO pin
//////////////////////////////////////////////////////////////////////

void
Matrix::set_meter(int gpio_pin) {
    meter_gpio = gpio_pin;

    gpio.start_clock(
        meter_gpio,
        GPIO::Oscillator,
        960,
        0,
        0,
        true);

    gpio.pwm_configure(
        meter_gpio,
        GPIO::PWM_Mode,
        false,
        0,
        false,
        false,
        GPIO::MSAlgorithm);
    gpio.pwm_enable(meter_gpio,true);
    gpio.pwm_ratio(meter_gpio,0,100);
}

//////////////////////////////////////////////////////////////////////
// Set meter deflection in percent
//////////////////////////////////////////////////////////////////////

void
Matrix::set_deflection(double pct) {
    gpio.pwm_ratio(meter_gpio,int(pct),100);
}

//////////////////////////////////////////////////////////////////////
// Display "bar" on a given row based on v07:
//  v07 == 0    Blank
//  v07 == 1    1 pixel
//  ..
//  v07 == 8    8 pixels
// RETURNS:
//  0       Successful
//  !=0     GPIO open failed
//////////////////////////////////////////////////////////////////////

int
Matrix::display(int row,int v07) {
    unsigned bar = 0;

    if ( errcode )
        return errcode;             // GPIO open failed
    else if ( v07 > 8 )
        v07 = 8;                    // Limit to 8 pixels

    for ( ; v07 > 0; --v07 )
	bar = (bar >> 1) | 0b10000000;

    data(row,bar);
    return 0;
}

int
Matrix::Pi() {
    static unsigned char pi[] = {
        0, 0b00000100, 0b00111100, 0b00000100,
        0b00000100, 0b00011100, 0b00100100, 0
    };

    if ( errcode )
        return errcode;             // GPIO open failed

    for ( int rx=0; rx < 8; ++rx )
        data(rx,pi[rx]);
    return 0;
}

// End matrix.cpp
