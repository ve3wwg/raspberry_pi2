//////////////////////////////////////////////////////////////////////
// matrix.hpp -- Matrix with MAX7219 Driver
// Date: Tue Mar 17 20:33:26 2015   (C) Warren Gay ve3wwg
///////////////////////////////////////////////////////////////////////

#ifndef MATRIX_HPP
#define MATRIX_HPP

#include "max7219.hpp"

class Matrix : public MAX7219 {
    int     errcode;                // errno if GPIO failed
    int     meter_gpio;

public:
    Matrix(int clk,int din,int load);
    ~Matrix();

    void set_meter(int gpio_pin);   // Configure 1 mA Meter
    void set_deflection(double pct);// Set meter deflection

    int display(int row,int v07);   // Display a bar
    int Pi();                       // Draw Pi
};


#endif // MATRIX_HPP

// End matrix.hpp
