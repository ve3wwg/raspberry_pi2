//////////////////////////////////////////////////////////////////////
// gpio.hpp -- GPIO class for direct access to GPIO pins
// Date: Sat Mar 14 22:38:48 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef GPIO_HPP
#define GPIO_HPP

#include <stdint.h>

#define GPIO_CLOCK  4   // Clocks only to GPIO # 4

typedef uint32_t volatile uint32_v;


class GPIO {
public:

    enum IO {       // GPIO Input or Output:
        Input=0,    // GPIO is to become an input pin
        Output=1,   // GPIO is to become an output pin
	Alt0=4,
	Alt1=5,
	Alt2=6,
	Alt3=7,
	Alt4=3,
	Alt5=2
    };

    enum Pull {     // GPIO Input Pullup resistor:
        None,       // No pull up or down resistor
        Up,         // Activate pullup resistor
        Down        // Activate pulldown resistor
    };

    enum Event {    // GPIO Input Detection:
        Rising=1,   // Rising edge detection
        Falling,    // Falling edge detection
        High,       // High level detection
        Low,            // Low level detection
        Async_Rising,   // Asynchronous Rising edge
        Async_Falling,  // Asynchronous Falling edge
    };

    enum PwmMode {  // PWM Mode:
        Serialize=0, // PWM is serialized
        PWM_Mode=1  // PWM mode
    };

    enum PwmAlgo {  // PWM Algorithm:
        PwmAlgorithm=0, // Use PWM algorithm
        MSAlgorithm=1   // Use MS algorithm
    };

    enum Source {   // GPIO Clock Source
        Gnd = 0,    // Grounded
        Oscillator=1, // Oscillator (19.2 MHz)
        PLLA=4,     // PLLA (Audio ~393.216 MHz)
        PLLC,       // 1000 MHz (changes with overclocking)
        PLLD,       // 500 MHz
        HDMI_Aux    // HDMI Aux (216 MHz?)
    };

    struct s_PWM_control {
        uint32_v    PWENx : 1;  // Channel enable
        uint32_v    MODEx : 1;  // 0=Serialise/1=PWM mode
        uint32_v    RPTLx : 1;  // 1 Last data in FIFO repeats
        uint32_v    SBITx : 1;  // State when no transmission
        uint32_v    POLAx : 1;  // 1=Inverted output
        uint32_v    USEFx : 1;  // 1=FIFO / 0=PWM_DATx
        uint32_v    MSENx : 1;  // 0=PWM algorithm/ 1=M/S transmission
    };

    struct s_PWM_status {
        uint32_t    fifo_full : 1;      // FIFO full flag
        uint32_t    fifo_empty : 1;     // FIFO empty flag
        uint32_t    fifo_werr : 1;      // FIFO error flag
        uint32_t    fifo_rerr : 1;      // FIFO read error flag
        uint32_t    gap_occurred : 1;   // Gap occurred flag
        uint32_t    bus_error : 1;      // Bus error flag
        uint32_t    chan_state : 1;     // Channel state
    };

private:

    bool        errcode;    // errno if GPIO open failed

public:

    GPIO();
    ~GPIO();

    inline int get_error() { return errcode; } // Test for error

    int configure(int gpio,IO io);      // Input/Output
    int configure(int gpio,Pull pull);  // None/Pullup/Pulldown
    int configure(int gpio,Event event,bool enable);
    int events_off(int gpio);           // Disable all input events

    int read_event(int gpio);		// Read event data
    int clear_event(int gpio);          // Reset event for gpio

    int read(int gpio);		        // Read GPIO into data
    int write(int gpio,int bit);        // Write GPIO

    uint32_t read_events();             // Read all 32 GPIO events
    uint32_t read();                    // Read all 32 GPIO bits

    int alt_function(int gpio,IO& io);
    int get_drive_strength(int gpio,bool& slew_limited,bool& hysteresis,int &drive);
    int set_drive_strength(int gpio,bool slew_limited,bool hysteresis,int drive);

    // PWM0: GPIO 12 or 18, PWM1: GPIO 13 or 19
    int pwm_configure(
        int gpio,
        PwmMode mode,
        bool repeat,
        int state,
        bool invert,
        bool fifo,
        PwmAlgo algorithm);
    int pwm_control(int gpio,s_PWM_control& control);
    int pwm_status(int gpio,s_PWM_status& status);
    int pwm_ratio(int gpio,uint32_t m,uint32_t s);
    int pwm_enable(int gpio,bool enable);
    int pwm_clear_status(int gpio,const s_PWM_status& status);
    int pwm_write_fifo(int gpio,uint32_t *data,size_t& n_words);
    bool pwm_fifo_full(int gpio);
    bool pwm_fifo_empty(int gpio);

    int get_pwm_ratio(int gpio,uint32_t& m,uint32_t& s);

    // Clock control: GPIO 4, 12, 18, 13 or 19
    int start_clock(
        int gpio,
        Source src,
        unsigned divi,
        unsigned divf,
        unsigned mash=0,
        bool on_gpio=true);
    int stop_clock(int gpio);

    int config_clock(
        int gpio,
        Source& src,
        unsigned& divi,
        unsigned& divf,
        unsigned& mash,
        bool& enabled);

    // Static methods
    static int pwm(int gpio,int& pwm,IO& altf);
    static void delay();  // A small delay for configuration
    static uint32_t peripheral_base();
    static const char *source_name(Source src);
    static const char *alt_name(IO io);
    static const char *gpio_alt_func(int gpio,IO io);
};

uint32_v *gpio_map_memory(off_t offset,size_t bytes);

#endif // GPIO_HPP

// End gpio.hpp
