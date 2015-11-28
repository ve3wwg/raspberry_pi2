///////////////////////////////////////////////////////////////////////
// gpio.cpp -- GPIO Class Implementation
// Date: Sat Mar 14 22:45:00 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////
//
// IMPORTANT NOTE:
//
//  This module makes use of the typedef
//
//      typedef uint32_t volatile uint32_v;
//
//  The uint32_v type is a uint32_t with the volatile attribute.
//  This is an important distinction to be aware of when working
//  with memory mapped peripheral registers.
//
///////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#include "gpio.hpp"
#include "mailbox.hpp"
#include "piutils.hpp"

#include <mutex>
#include <unordered_map>

//////////////////////////////////////////////////////////////////////
// Peripheral Base Macros
//////////////////////////////////////////////////////////////////////

#define BCM2708_PERI_BASE    	0x3F000000 	// Assumed for RPi2
#define GPIO_BASE_OFFSET	0x200000	// 0x7E20_0000
#define PADS_BASE_OFFSET        0x100000        // 0x7E10_0000
#define CLOCK_BASE_OFFSET       0x101000	// 0x7E10_1000
#define PWM_BASE_OFFSET		0x20C000	// 0x7E20_C000

//////////////////////////////////////////////////////////////////////
// GPIO Macros
//////////////////////////////////////////////////////////////////////

#define GPIOOFF(o)	(((o)-0x7E000000-GPIO_BASE_OFFSET)/sizeof(uint32_t))
#define GPIOREG(o)	(*(ugpio+GPIOOFF(o)))
#define GPIOREG2(o,wo)	(*(ugpio+GPIOOFF(o)+(wo)))

#define GPIO_GPFSEL0	0x7E200000 
#define GPIO_GPSET0	0x7E20001C
#define GPIO_GPCLR0	0x7E200028 
#define GPIO_GPLEV0     0x7E200034 

#define GPIO_GPEDS0 	0x7E200040 
#define GPIO_GPREN0	0x7E20004C 
#define GPIO_GPFEN0     0x7E200058 
#define GPIO_GPHEN0     0x7E200064 
#define GPIO_GPLEN0     0x7E200070 

#define GPIO_GPAREN0	0x7E20007C 
#define GPIO_GPAFEN0 	0x7E200088 

#define GPIO_GPPUD	0x7E200094
#define GPIO_GPUDCLK0	0x7E200098
#define GPIO_GPUDCLK1	0x7E200098

#define PADSOFF(o)	(((o)-0x7E000000-PADS_BASE_OFFSET)/sizeof(uint32_t))
#define PADSREG(o,x)	(*(upads+PADSOFF(o)+x))

#define GPIO_PADS00_27	0x7E10002C
#define GPIO_PADS28_45	0x7E100030 

//////////////////////////////////////////////////////////////////////
// Clock Peripherals
//////////////////////////////////////////////////////////////////////
 
#define CLKOFF(o,w)	(((o)-0x7E000000-CLOCK_BASE_OFFSET+(w))/sizeof(uint32_t))
#define CLKCTL(o)	(*(u_CM_XXXCTL*)(uclock+CLKOFF(o,0)))
#define CLKDIV(o)	(*(u_CM_XXXDIV*)(uclock+CLKOFF(o,0)))

#define DMABASE         0x7E007000

#define CM_GP0CTL       0x7E101070
#define CM_GP0DIV       0x7E101074
// #define CM_PCMCTL    0x7E101098
// #define CM_PCMDIV    0x7E10109C 
#define CM_PWMCTL	0X7E1010A0
#define CM_PWMDIV	0X7E1010A4
#define PWM_OFFSET	0x20

//////////////////////////////////////////////////////////////////////
// PWM Registers
//////////////////////////////////////////////////////////////////////

#define PWMOFF(o)	(((o)-0x7E000000-PWM_BASE_OFFSET)/sizeof(uint32_t))
#define PWMCTL(o)	(*(u_PWM_CTL*)(upwm+PWMOFF(o)))
#define PWMSTA(o)	(*(u_PWM_STA*)(upwm+PWMOFF(o)))
#define PWMRNG(o)	(*(u_PWM_RNG*)(upwm+PWMOFF(o)))
#define PWMDAT(o)	(*(u_PWM_DAT*)(upwm+PWMOFF(o)))
#define PWMFIFO(o)	(*(u_PWM_FIFO*)(upwm+PWMOFF(o)))
#define PWMDMAC(o)      (*(u_PWM_DMAC*)(upwm+PWMOFF(o)))

#define PWM_CTL         0x7e20c000
#define PWM_STA         0x7e20c004
#define PWM_DMAC        0x7e20c008
#define PWM_RNG1        0x7e20c010
#define PWM_DAT1        0x7e20c014
#define PWM_FIF1        0x7e20c018
#define PWM_RNG2        0x7e20c020
#define PWM_DAT2        0x7e20c024

//////////////////////////////////////////////////////////////////////
// Clock Registers
//////////////////////////////////////////////////////////////////////

union u_CM_XXXCTL {		// CM_GPxCTL or CM_PWMCTL
    struct s_CM_XXXCTL {
        uint32_v    SRC : 4;    // Clock source
        uint32_v    ENAB : 1;   // Enable the clock generator
        uint32_v    KILL : 1;   // Stop and reset clock generator
        uint32_v    : 1;
        uint32_v    BUSY : 1;   // busy when set
        uint32_v    FLIP : 1;   // Invert the clock
        uint32_v    MASH : 2;   // MASH control
        uint32_v    : 13;
        uint32_v    PASSWD : 8; // 0x5A
    }           s;              // As struct
    uint32_v    u;              // As unsigned
};

union u_CM_XXXDIV {		// CM_GPxDIV or CM_PWMDIV
    struct s_CM_XXXDIV {
        uint32_v    DIVF : 12;  // Franctional part of divisor (DIVF)
        uint32_v    DIVI : 12;  // Integer part of divisor (DIVI)
        uint32_v    PASSWD : 8; // 0x5A
    }           s;              // As struct
    uint32_v    u;              // As unsigned
};

union u_PWM_CTL {
    struct s_PWM_CTL {
        uint32_v    PWEN1 : 1;  // Channel 1 enable
        uint32_v    MODE1 : 1;  // 0=Serialise/1=PWM mode
        uint32_v    RPTL1 : 1;  // 1 Last data in FIFO repeats
        uint32_v    SBIT1 : 1;  // State when no transmission
        uint32_v    POLA1 : 1;  // 1=Inverted output
        uint32_v    USEF1 : 1;  // 1=FIFO / 0=PWM_DATx
        uint32_v    CLRF1 : 1;  // 1=Clear FIFO / else no effect
        uint32_v    MSEN1 : 1;  // 0=PWM algorithm/ 1=M/S transmission
        uint32_v    PWEN2 : 1;  // Channel 2 enable
        uint32_v    MODE2 : 1;  // etc.
        uint32_v    RPTL2 : 1;
        uint32_v    SBIT2 : 1;
        uint32_v    POLA2 : 1;
        uint32_v    USEF2 : 1;
        uint32_v    mbz1 : 1;   // Reserved: write as 0, read=x
        uint32_v    MSEN2 : 1;
        uint32_v    mbz2 : 16;  // Reserved: write as 0, read=x
    }           s;              // As struct
    uint32_v    u;              // As uint32_t
};

union u_PWM_STA {
    struct s_PWM_STA {
        uint32_v    FULL1 : 1;  // FIFO full flag
        uint32_v    EMPT1 : 1;  // FIFO empty flag
        uint32_v    WERR1 : 1;  // FIFO error flag
        uint32_v    RERR1 : 1;  // FIFO read error flag
        uint32_v    GAPO1 : 1;  // Channel 1 gap occurred flag
        uint32_v    GAPO2 : 1;  // Channel 2 gap occurred flag
        uint32_v    GAPO3 : 1;  // N/A
        uint32_v    GAPO4 : 1;  // N/A
        uint32_v    BERR : 1;   // Bus error flag
        uint32_v    STA1 : 1;   // Channel 1 state
        uint32_v    STA2 : 1;   // Channel 2 state
        uint32_v    STA3 : 1;   // N/A
        uint32_v    STA4 : 1;   // N/A
        uint32_v    mbz1 : 19;  // Write as 0, read=x
    }           s;              // As struct
    uint32_v            u;      // As uint32_t
};

union u_PWM_DMAC {
    struct s_PWM_DMAC {
        uint32_v    DREQ : 8;
        uint32_v    PANIC : 8;
        uint32_v    : 15;
        uint32_v    ENAB : 1;
    }           s;
    uint32_v    u;
};

union u_PWM_RNG {
    uint32_v    u;              // PWM_RNGx Channel x Range
};

union u_PWM_DAT {               // When USEFx=1
    uint32_v    u;              // PWM_DATx Channel x Data
};

union u_PWM_FIFO {
    uint32_v    u;              // PWM_FIFx Channel x Input
};

//////////////////////////////////////////////////////////////////////
// Static members of the GPIO Class
//////////////////////////////////////////////////////////////////////

static uint32_v *ugpio = 0;     // Pointer to Peripheral space
static uint32_v *upads = 0;	// Pointer to GPIO pad space
static uint32_v *uclock = 0;    // Pointer to CM_GP0CTL space
static uint32_v *upwm = 0;	// Pointer to PWM_CTL space
static std::mutex uglock;       // Mutex for ugpio
static int usage_count = 0;
static uint32_t block_size = 0; // System block size

static std::unordered_map<int,std::string> gpio_alt0 = {
    { 0, "SDA0" }, { 1, "SCL0" }, { 2, "SDA1" }, { 3, "SCL1" },
    { 4, "GPCLK0" }, { 5, "GPCLK1" }, { 6, "GPCLK2" },
    { 7, "SPI0_CE1_N" }, { 8, "SPI0_CE0_N" }, { 9, "SPI0_MISO" },
    { 10, "SPI0_MOSI" }, { 11, "SPI0_SCLK" }, { 12, "PWM0" },
    { 13, "PWM1" }, { 14, "TXD0" }, { 15, "RXD0" },
    { 16, "(reserved)" }, { 17, "(reserved)" }, { 18, "PCM_CLK" },
    { 19, "PCM_FS" }, { 20, "PCM_DIN" }, { 21, "PCM_DOUT" },
    { 22, "(reserved)" }, { 23, "(reserved)" }, { 24, "(reserved)" },
    { 25, "(reserved)" }, { 26, "(reserved)" }, { 27, "(reserved)" },
    { 28, "SDA0" }, { 29, "SCL0" }, { 30, "(reserved)" },
    { 31, "(reserved)" }};

static std::unordered_map<int,std::string> gpio_alt1 = {
    { 0, "SA5" }, { 1, "SA4" }, { 2, "SA3" }, { 3, "SA2" }, { 4, "SA1" },
    { 5, "SA0" }, { 6, "SOE_N/SE" }, { 7, "SWE_N/SRW_N" }, { 8, "SD0" },
    { 9, "SD1" }, { 10, "SD2" }, { 11, "SD3" }, { 12, "SD4" }, 
    { 13, "SD5" }, { 14, "SD6" }, { 15, "SD7" }, { 16, "SD8" }, 
    { 17, "SD9" }, { 18, "SD10" }, { 19, "SD11" }, { 20, "SD12" },
    { 21, "SD13" }, { 22, "SD14" }, { 23, "SD15" }, { 24, "SD16" },
    { 25, "SD17" }, { 26, "(reserved)" }, { 27, "(reserved)" },
    { 28, "SA5" }, { 29, "SA4" }, { 30, "SA3" }, { 31, "SA2" } };

static std::unordered_map<int,std::string> gpio_alt2 = {
    { 0, "(reserved)" }, { 1, "(reserved)" }, { 2, "(reserved)" },
    { 3, "(reserved)" }, { 4, "(reserved)" }, { 5, "(reserved)" },
    { 6, "(reserved)" }, { 7, "(reserved)" }, { 8, "(reserved)" },
    { 9, "(reserved)" }, { 10, "(reserved)" }, { 11, "(reserved)" },
    { 12, "(reserved)" }, { 13, "(reserved)" }, { 14, "(reserved)" },
    { 15, "(reserved)" }, { 16, "(reserved)" }, { 17, "(reserved)" },
    { 18, "(reserved)" }, { 19, "(reserved)" }, { 20, "(reserved)" },
    { 21, "(reserved)" }, { 22, "(reserved)" }, { 23, "(reserved)" },
    { 24, "(reserved)" }, { 25, "(reserved)" }, { 26, "(reserved)" },
    { 27, "(reserved)" }, { 28, "PCM_CLK" }, { 29, "PCM_FS" },
    { 30, "PCM_DIN" }, { 31, "PCM_DOUT" } };

static std::unordered_map<int,std::string> gpio_alt3 = {
    { 0, "-" }, { 1, "-" }, { 2, "-" }, { 3, "-" }, { 4, "-" },
    { 5, "-" }, { 6, "-" }, { 7, "-" }, { 8, "-" }, { 9, "-" },
    { 10, "-" }, { 11, "-" }, { 12, "-" }, { 13, "-" }, { 14, "-" },
    { 15, "-" }, { 16, "CTS0" }, { 17, "RTS0" }, 
    { 18, "BSCSL SDA/MOSI" }, { 19, "BSCSL SCL/SCLK" },
    { 20, "BSCSL/MISO" }, { 21, "BSCSL/CE_N" }, { 22, "SD1_CLK" },
    { 23, "SD1_CMD" }, { 24, "SD1_DAT0" }, { 25, "SD1_DAT1" },
    { 26, "SD1_DAT2" }, { 27, "SD1_DAT3" }, { 28, "<res>" },
    { 29, "(reserved)" }, { 30, "CTS0" }, { 31, "RTS0" } };

static std::unordered_map<int,std::string> gpio_alt4 = {
    { 0, "-" }, { 1, "-" }, { 2, "-" }, { 3, "-" }, { 4, "-" },
    { 5, "-" }, { 6, "-" }, { 7, "-" }, { 8, "-" }, { 9, "-" }, 
    { 10, "-" }, { 11, "-" }, { 12, "-" }, { 13, "-" }, 
    { 14, "-" }, { 15, "-" }, { 16, "SPI1_CE2_N" },
    { 17, "SPI1_CE1_N" }, { 18, "SPI1_CE0_N" }, 
    { 19, "SPI1_MISO" }, { 20, "SPI1_MOSI" },
    { 21, "SPI1_SCLK" }, { 22, "ARM_TRST" },
    { 23, "ARM_RTCK" }, { 24, "ARM_TDO" }, { 25, "ARM_TCK" },
    { 26, "ARM_TDI" }, { 27, "ARM_TMS" }, { 28, "-" },
    { 29, "-" }, { 30, "-" }, { 31, "-" } };

static std::unordered_map<int,std::string> gpio_alt5 = {
    { 0, "-" }, { 1, "-" }, { 2, "-" }, { 3, "-" }, { 4, "ARM_TDI" },
    { 5, "ARM_TDO" }, { 6, "ARM_RTCK" }, { 7, "-" }, { 8, "-" },
    { 9, "-" }, { 10, "-" }, { 11, "-" }, { 12, "ARM_TMS" },
    { 13, "ARM_TCK" }, { 14, "TXD1" }, { 15, "RXD1" }, 
    { 16, "CTS1" }, { 17, "RTS1" }, { 18, "PWM0" }, { 19, "PWM1" },
    { 20, "GPCLK0" }, { 21, "GPCLK1" }, { 22, "-" }, { 23, "-" },
    { 24, "-" }, { 25, "-" }, { 26, "-" }, { 27, "-" }, { 28, "-" },
    { 29, "-" }, { 30, "CTS1" }, { 31, "RTS1" } };

static std::unordered_map<int,std::unordered_map<int,std::string>*>
    gpio_alts = {
        { GPIO::Alt0, &gpio_alt0},
        { GPIO::Alt1, &gpio_alt1},
        { GPIO::Alt2, &gpio_alt2},
        { GPIO::Alt3, &gpio_alt3},
        { GPIO::Alt4, &gpio_alt4},
        { GPIO::Alt5, &gpio_alt5}
    };

//////////////////////////////////////////////////////////////////////
// Inline functions to calculate GPIO offset and shift
//////////////////////////////////////////////////////////////////////

inline uint32_v&
set_gpio10(int gpio,int& shift,uint32_t base) {
	uint32_t offset = gpio / 10;
	shift = gpio % 10 * 3;
	return GPIOREG2(base,offset);
}

inline uint32_v&
set_gpio32(int gpio,int& shift,uint32_t base) {
    uint32_t offset = gpio / 32;
    shift = gpio % 32;
    return GPIOREG2(base,offset);
}

//////////////////////////////////////////////////////////////////////
// GPIO Class Constructor
//////////////////////////////////////////////////////////////////////

GPIO::GPIO() {

    errcode = 0;

    // Map memory if not done already
    uglock.lock();

    if ( !ugpio ) {             // If not already mapped
        uint32_t peri_base = 0;
	
        if ( !block_size ) {
            block_size = sys_page_size();
            assert(block_size = 4096);
        }

        peri_base = GPIO::peripheral_base();

        ugpio = (uint32_v *)Mailbox::map(peri_base+GPIO_BASE_OFFSET,block_size);
        if ( !ugpio ) {
            errcode = errno;
            uglock.unlock();
            return;             // Failed
        }

	upads = (uint32_v *)Mailbox::map(peri_base+PADS_BASE_OFFSET,block_size);
        upwm = (uint32_v *)Mailbox::map(peri_base+PWM_BASE_OFFSET,block_size);
        uclock = (uint32_v *)Mailbox::map(peri_base+CLOCK_BASE_OFFSET,block_size);
    }

    ++usage_count;              // Successful
    uglock.unlock();
}

//////////////////////////////////////////////////////////////////////
// Destructor
//////////////////////////////////////////////////////////////////////

GPIO::~GPIO() {
    uglock.lock();
    if ( --usage_count <= 0 ) {
        if ( ugpio ) {
            Mailbox::unmap((void *)ugpio,block_size);
            ugpio = 0;
        }
        if ( upads ) {
            Mailbox::unmap((void *)upads,block_size);
            upads = 0;
        }
        if ( upwm ) {
            Mailbox::unmap((void *)upwm,block_size);
	    upwm = 0;
        }
        if ( uclock ) {
	    Mailbox::unmap((void *)uclock,block_size);
	    uclock = 0;
        }
    }
    uglock.unlock();
}

//////////////////////////////////////////////////////////////////////
// Configure a GPIO pin as Input or Output
//////////////////////////////////////////////////////////////////////

int
GPIO::configure(int gpio,IO io) {
    int shift;
    int alt = int(io) & 0x07;

    if ( errcode )
        return errcode;         // /dev/mem open failed
    else if ( gpio < 0 )
        return EINVAL;          // Invalid parameter

    uint32_v& gpiosel = set_gpio10(gpio,shift,GPIO_GPFSEL0);
    gpiosel = (gpiosel & ~(7<<shift)) | (alt<<shift);	
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Return currently configured Alternate function for gpio
//////////////////////////////////////////////////////////////////////

int
GPIO::alt_function(int gpio,IO& io) {
    int shift;

    if ( errcode )
        return errcode;         // /dev/mem open failed
    else if ( gpio < 0 )
        return EINVAL;          // Invalid parameter

    uint32_v& gpiosel = set_gpio10(gpio,shift,GPIO_GPFSEL0);
    uint32_t r = (gpiosel >> shift) & 7;

    io = IO(r);
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Return the drive strength of a given gpio pin
//////////////////////////////////////////////////////////////////////

int
GPIO::get_drive_strength(int gpio,bool& slew_limited,bool& hysteresis,int &drive) {

    if ( errcode )
        return errcode;         // /dev/mem open failed
    else if ( gpio < 0 || gpio > 53 )
        return EINVAL;          // Invalid parameter

    uint32_t padx = gpio / 28;
    uint32_v& padreg = PADSREG(GPIO_PADS00_27,padx);

    drive = padreg & 7;
    hysteresis = (padreg & 0x0008) ? true : false;
    slew_limited = (padreg & 0x0010) ? true : false;
    return 0;
}

int
GPIO::set_drive_strength(int gpio,bool slew_limited,bool hysteresis,int drive) {

    if ( errcode )
        return errcode;         // /dev/mem open failed
    else if ( gpio < 0 || gpio > 53 )
        return EINVAL;          // Invalid parameter

    uint32_t padx = gpio / 28;
    uint32_v& padreg = PADSREG(GPIO_PADS00_27,padx);

    uint32_t config = 0x5A000000;
    if ( slew_limited )
        config |= 1 << 4;
    if ( hysteresis )
        config |= 1 << 3;
    config |= drive & 7;

    padreg = config;
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Configure a GPIO pin to have None/Pullup/Pulldown resistor
//////////////////////////////////////////////////////////////////////

int
GPIO::configure(int gpio,Pull pull) {
    
    if ( errcode )
        return errcode;             // /dev/mem open failed
    else if ( gpio < 0 || gpio >= 32 )
        return EINVAL;              // Invalid parameter

    uint32_t mask = 1 << gpio;      // GPIOs 0 to 31 only
    uint32_t pmask;

    switch ( pull ) {
    case None :
        pmask = 0;                  // No pullup/down
        break;
    case Up :
        pmask = 0b10;               // Pullup resistor
        break;
    case Down :
        pmask = 0b01;               // Pulldown resistor
        break;
    };

    uint32_v& GPPUD = GPIOREG(GPIO_GPPUD);
    uint32_v& GPUDCLK0 = GPIOREG(GPIO_GPUDCLK0);

    GPPUD = pmask;                  // Select pullup setting
    GPIO::delay();
    GPUDCLK0 = mask;                // Set the GPIO of interest
    GPIO::delay();
    GPPUD = 0;                      // Reset pmask
    GPIO::delay();
    GPUDCLK0 = 0;                   // Set the GPIO of interest
    GPIO::delay();

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Configure edge/level detection enable/disable
//////////////////////////////////////////////////////////////////////

int
GPIO::clear_event(int gpio) {
    int shift;

    if ( errcode != 0 )
        return errcode;             // /dev/mem did not open
    else if ( gpio < 0 )
        return EINVAL;

    uint32_v& gpeds0 = set_gpio32(gpio,shift,GPIO_GPEDS0);

    gpeds0 |= (1 << shift);
    GPIO::delay();
    gpeds0 = 0;

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Enable/Disable edge/level detection
//////////////////////////////////////////////////////////////////////

int
GPIO::configure(int gpio,Event event,bool enable) {
    int shift;
    uint32_t base;

    if ( errcode != 0 )
        return errcode;             // /dev/mem did not open
    else if ( gpio < 0 )
        return EINVAL;

    switch ( event ) {
    case Rising:
        base = GPIO_GPREN0;
        break;
    case Falling:
        base = GPIO_GPFEN0;
        break;
    case High:
        base = GPIO_GPHEN0;
        break;
    case Low:
        base = GPIO_GPLEN0;
        break;
    case Async_Rising:
        base = GPIO_GPAREN0;
        break;
    case Async_Falling:
        base = GPIO_GPAFEN0;
        break;
    }

    uint32_v& gplen = set_gpio32(gpio,shift,base);

    if ( enable )
        gplen |= (1 << shift);
    else
        gplen &= ~(1 << shift);

    clear_event(gpio);

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Unconfigure all input events
//////////////////////////////////////////////////////////////////////

int
GPIO::events_off(int gpio) {

    if ( errcode != 0 )
        return errcode;             // /dev/mem did not open
    else if ( gpio < 0 )
        return EINVAL;

    configure(gpio,Rising,false);
    configure(gpio,Falling,false);
    configure(gpio,High,false);
    configure(gpio,Low,false);
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Return event triggered data
//////////////////////////////////////////////////////////////////////

int
GPIO::read_event(int gpio) {
    int shift, event;

    if ( errcode != 0 )
        return errcode;             // /dev/mem did not open
    else if ( gpio < 0 )
        return EINVAL;

    uint32_v& gpeds0 = set_gpio32(gpio,shift,GPIO_GPEDS0);

    event = !!(gpeds0 & (1 << shift));

    if ( event ) 
        clear_event(gpio);

    return event;
}

//////////////////////////////////////////////////////////////////////
// Read all GPIO events at once
//
// Note: No events are cleared by this call.
//////////////////////////////////////////////////////////////////////

uint32_t
GPIO::read_events() {

    if ( errcode != 0 )
        return errcode;             // /dev/mem did not open

    uint32_v& gpeds0 = GPIOREG(GPIO_GPEDS0);
    return gpeds0;
}

//////////////////////////////////////////////////////////////////////
// Read a GPIO pin
//////////////////////////////////////////////////////////////////////

int
GPIO::read(int gpio) {
    int shift;
    
    if ( errcode != 0 )
        return errcode;             // /dev/mem did not open
    else if ( gpio < 0 || gpio > 31 )
        return EINVAL;

    uint32_v& gpiolev = set_gpio32(gpio,shift,GPIO_GPLEV0);

    return !!(gpiolev & (1<<shift));
}

//////////////////////////////////////////////////////////////////////
// Write a GPIO pin
//////////////////////////////////////////////////////////////////////

int
GPIO::write(int gpio,int bit) {
    int shift;
    
    if ( bit ) {
        uint32_v& gpioset = set_gpio32(gpio,shift,GPIO_GPSET0);
        gpioset = 1u << shift;
    } else {
        uint32_v& gpioclr = set_gpio32(gpio,shift,GPIO_GPCLR0);
        gpioclr = 1u << shift;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Read/Write all GPIO bits at once (this can segfault if not opened)
//////////////////////////////////////////////////////////////////////

uint32_t
GPIO::read() {

    if ( errcode != 0 )
        return errcode;             // /dev/mem did not open

    uint32_v& gpiolev = GPIOREG(GPIO_GPLEV0);

    return gpiolev;
}

//////////////////////////////////////////////////////////////////////
// A short configuration delay
//////////////////////////////////////////////////////////////////////

void
GPIO::delay() {
    for ( int i=0; i<150; i++ ) {
        asm volatile("nop");
    }
}

//////////////////////////////////////////////////////////////////////
// Start clock on GPIO 4
//////////////////////////////////////////////////////////////////////

int
GPIO::start_clock(
  int gpio,
  Source src,
  unsigned divi,
  unsigned divf,
  unsigned mash,
  bool on_gpio) {
    u_CM_XXXCTL *ctlp = 0;
    u_CM_XXXDIV *divp = 0;
    int rc, pwmx;
    IO altf;

    if ( errcode )
        return errcode;         // Failed mmap/open

    if ( gpio == GPIO_CLOCK ) {
        // GP0 clock
        ctlp = &CLKCTL(CM_GP0CTL);
        divp = &CLKDIV(CM_GP0DIV);
        altf = Alt0;
    } else {
        // PWM clock
        if ( (rc = pwm(gpio,pwmx,altf)) != 0 )
            return rc;

        ctlp = &CLKCTL(CM_PWMCTL);
        divp = &CLKDIV(CM_PWMDIV);
    }

    u_CM_XXXCTL& regctl = *ctlp;    // Control Reg
    u_CM_XXXDIV& regdiv = *divp;    // Div Reg
    u_CM_XXXCTL tmpctl;
    u_CM_XXXDIV tmpdiv;
        
    if ( on_gpio ) {
        // Configure the GPIO for GP0/PWM
	rc = configure(gpio,altf);
        if ( rc )
            return rc;
    } 

    // Stop the clock
    if ( regctl.s.BUSY || regctl.s.ENAB ) {
        stop_clock(gpio);
        while ( regctl.s.BUSY )
            ;
    }
    assert(!regctl.s.BUSY && !regctl.s.ENAB );

    // Configure the clock
    tmpctl.u = 0;
    tmpctl.s.SRC = unsigned(src);
    tmpctl.s.ENAB = 0;
    tmpctl.s.FLIP = 0;
    tmpctl.s.MASH = mash;
    tmpctl.s.PASSWD = 0x5A;
    regctl.u = tmpctl.u;       // Configured but not enabled

    uswait(100);

    while ( regctl.s.BUSY )
        ;

    tmpdiv.u = 0u;
    tmpdiv.s.DIVF = divf;
    tmpdiv.s.DIVI = divi;
    tmpdiv.s.PASSWD = 0x5A;
    regdiv.u = tmpdiv.u;

    uswait(100);

    tmpctl.u = 0;
    tmpctl.s.SRC = unsigned(src);
    tmpctl.s.MASH = mash;
    tmpctl.s.ENAB = 1;
    tmpctl.s.PASSWD = 0x5A;
    regctl.u = tmpctl.u;

    uswait(100);

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Return current configuration for clock
//////////////////////////////////////////////////////////////////////

int
GPIO::config_clock(
  int gpio,
  Source& src,
  unsigned& divi,
  unsigned& divf,
  unsigned& mash,
  bool& enabled) {

    u_CM_XXXCTL *ctlp = 0;
    u_CM_XXXDIV *divp = 0;
    int rc, pwmx;
    IO altf;

    if ( errcode )
        return errcode;         // Failed mmap/open

    if ( gpio == GPIO_CLOCK ) {
        // GP0 clock
        ctlp = &CLKCTL(CM_GP0CTL);
        divp = &CLKDIV(CM_GP0DIV);
        altf = Alt0;
    } else {
        // PWM clock
        if ( (rc = pwm(gpio,pwmx,altf)) != 0 )
            return rc;

        ctlp = &CLKCTL(CM_PWMCTL);
        divp = &CLKDIV(CM_PWMDIV);
    }

    u_CM_XXXCTL& regctl = *ctlp;    // Control Reg
    u_CM_XXXDIV& regdiv = *divp;    // Div Reg

    u_CM_XXXCTL tmpctl = regctl;    // Copy
    u_CM_XXXDIV tmpdiv = regdiv;    // .. values
        
    src = Source(tmpctl.s.SRC);
    enabled = !!tmpctl.s.ENAB;
    divi = tmpdiv.s.DIVI;
    divf = tmpdiv.s.DIVF;
    mash = tmpctl.s.MASH;
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Stop a clock on GPIO 4
//////////////////////////////////////////////////////////////////////

int
GPIO::stop_clock(int gpio) {
    u_CM_XXXCTL *ctlp = 0;
    int rc, pwmx = -1;
    IO altf;

    if ( errcode )
        return errcode;             // Failed mmap/open

    if ( gpio == GPIO_CLOCK ) {
        // GP0 clock
        ctlp = &CLKCTL(CM_GP0CTL);
        altf = Alt0;
    } else  {
        // PWM clock
        if ( (rc = pwm(gpio,pwmx,altf)) != 0 )
            return rc;
        ctlp = &CLKCTL(CM_PWMCTL);
    }

    u_CM_XXXCTL& regctl = *ctlp;    // CTL Reg
    u_CM_XXXCTL tmpctl;

    tmpctl.u = 0;
    tmpctl.s.KILL = 1;              // Seems that this is necessary
    tmpctl.s.ENAB = 0;
    tmpctl.s.PASSWD = 0x5A;
    regctl.u = tmpctl.u;

    while ( regctl.s.BUSY )
        ;

    uswait(10);    
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Utility: Return the PWM unit # based upon gpio
//////////////////////////////////////////////////////////////////////

int
GPIO::pwm(int gpio,int& pwm,IO& altf) {

    switch ( gpio ) {
    case 12:
        pwm = 0;                    // PWM 0
        altf = Alt0;
        break;
    case 18:
        pwm = 0;                    // PWM 0
        altf = Alt5;
        break;
    case 13:
        pwm = 1;                    // PWM 1
        altf = Alt0;
        break;
    case 19:
        pwm = 1;                    // PWM 1
        altf = Alt5;
        break;
    default:
	pwm = 0;
	altf = Alt0;
        return EINVAL;              // Only have PWM 0 and 1 available
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Configure the PWM Peripheral.
//
// Notes:
//	1. It is assumed that the clock for the gpio has already
//	   been started (PWM needs it to configure/operate).
//	2. usleep(10) is used here instead of GPIO::delay(), since
//	   a longer delay is required by the PWM peripheral. Some
//	   of this may be attributed to the PWM clock rate.
//	3. If you experience pwmsta.s.BERR, then this indicates that
//	   longer delays are required (or a coding problem).
//////////////////////////////////////////////////////////////////////

int
GPIO::pwm_configure(int gpio,PwmMode mode,bool repeat,int state,bool invert,bool fifo,PwmAlgo algorithm) {
    u_PWM_CTL& regctl = PWMCTL(PWM_CTL);
    u_PWM_STA& regsta = PWMSTA(PWM_STA);
    u_PWM_DMAC& regdmac = PWMDMAC(PWM_DMAC);
    u_PWM_CTL tmpctl;
    u_PWM_STA tmpsta;
    int pwmx, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwmx,altf)) != 0 )
        return rc;                  // Return error

    regdmac.s.ENAB = 0;             // Disable DMA

    //////////////////////////////////////////////////////////////////
    // Disable the PWM peripheral
    //////////////////////////////////////////////////////////////////

    switch ( pwmx ) {
    case 0:
	if ( regctl.s.PWEN1 ) {
		regctl.s.PWEN1 = 0;
		while ( regsta.s.STA1 )
			;
	}
        break;
    case 1:
	if ( regctl.s.PWEN2 ) {
		regctl.s.PWEN2 = 0;
		while ( regsta.s.STA2 )
			;
	}
        break;    
    default:
        assert(0);;
    }

    GPIO::delay();

    //////////////////////////////////////////////////////////////////
    // Clear any error flags
    //////////////////////////////////////////////////////////////////

    if ( regsta.s.BERR || regsta.s.RERR1 || regsta.s.WERR1 ) {
        tmpsta.u = 0;
        if ( regsta.s.BERR )
            tmpsta.s.BERR = 1;      // Reset Bus Error
        if ( regsta.s.RERR1 )
            tmpsta.s.RERR1 = 1;     // Clear FIFO read error
        if ( regsta.s.WERR1 )
            tmpsta.s.WERR1 = 1;     // Clear FIFO write error
        regsta.u = tmpsta.u;
        GPIO::delay();
    }

    //////////////////////////////////////////////////////////////////
    // Reconfigure the PWM Peripheral
    //////////////////////////////////////////////////////////////////

    tmpctl.u = regctl.u;

    switch ( pwmx ) {
    case 0:
        tmpctl.s.MSEN1 = algorithm == MSAlgorithm ? 1 : 0;
        tmpctl.s.USEF1 = fifo ? 1 : 0;
        tmpctl.s.POLA1 = invert ? 1 : 0;
        tmpctl.s.SBIT1 = !!state;
        tmpctl.s.RPTL1 = repeat ? 1 : 0;
	tmpctl.s.MODE1 = mode == PWM_Mode ? 0 : 1;
        break;
    case 1:
        tmpctl.s.MSEN2 = algorithm == MSAlgorithm ? 1 : 0;
        tmpctl.s.USEF2 = fifo ? 1 : 0;
        tmpctl.s.POLA2 = invert ? 1 : 0;
        tmpctl.s.SBIT2 = !!state;
        tmpctl.s.RPTL2 = repeat ? 1 : 0;
	tmpctl.s.MODE2 = mode == PWM_Mode ? 0 : 1;
        break;    
    default:
        assert(0);;
    }

    tmpctl.s.CLRF1 = 1;                // Clear FIFO
    regctl.u = tmpctl.u;
    usleep(10);

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Return status for PWM on gpio
//////////////////////////////////////////////////////////////////////

int
GPIO::pwm_status(int gpio,s_PWM_status& status) {
    u_PWM_STA& regsta = PWMSTA(PWM_STA);
    u_PWM_STA tmpsta;
    int pwmx, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwmx,altf)) != 0 )
        return rc;

    tmpsta.u = regsta.u;
    
    status.fifo_full = tmpsta.s.FULL1;
    status.fifo_empty = tmpsta.s.EMPT1;
    status.fifo_werr = tmpsta.s.WERR1;
    status.fifo_rerr = tmpsta.s.RERR1;
    status.bus_error = tmpsta.s.BERR;

    switch ( pwmx ) {
    case 0:
        status.gap_occurred = tmpsta.s.GAPO1;
        status.chan_state = tmpsta.s.STA1;
        break;
    case 1:
        status.gap_occurred = tmpsta.s.GAPO2;
        status.chan_state = tmpsta.s.STA2;
        break;
    default:
        return EINVAL;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Return the control configuration for PWM
//////////////////////////////////////////////////////////////////////

int
GPIO::pwm_control(int gpio,s_PWM_control& control) {
    u_PWM_CTL& regctl = PWMCTL(PWM_CTL);
    IO altf;
    int pwmx, rc;

    if ( (rc = GPIO::pwm(gpio,pwmx,altf)) != 0 )
        return rc;                  // Return error

    switch ( pwmx ) {
    case 0:
        control.PWENx = regctl.s.PWEN1;
        control.MODEx = regctl.s.MODE1;
        control.RPTLx = regctl.s.RPTL1;
        control.SBITx = regctl.s.SBIT1;
        control.POLAx = regctl.s.POLA1;
        control.USEFx = regctl.s.USEF1;
        control.MSENx = regctl.s.MSEN1;
        break;
    case 1:
        control.PWENx = regctl.s.PWEN2;
        control.MODEx = regctl.s.MODE2;
        control.RPTLx = regctl.s.RPTL2;
        control.SBITx = regctl.s.SBIT2;
        control.POLAx = regctl.s.POLA2;
        control.USEFx = regctl.s.USEF2;
        control.MSENx = regctl.s.MSEN2;
        break;
    default:
        assert(0);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Set the M and S for PWM
//////////////////////////////////////////////////////////////////////

int
GPIO::pwm_ratio(int gpio,uint32_t m,uint32_t s) {
    int pwmx, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwmx,altf)) != 0 )
        return rc;                  // Return error

    switch ( pwmx ) {
    case 0:
        {
            u_PWM_RNG& regrng = PWMRNG(PWM_RNG1);
            u_PWM_DAT& regdat = PWMDAT(PWM_DAT1);

            regrng.u = s;
            regdat.u = m;
        }
        break;
    case 1:
        {
            u_PWM_RNG& regrng = PWMRNG(PWM_RNG2);
            u_PWM_DAT& regdat = PWMDAT(PWM_DAT2);

            regrng.u = s;
            regdat.u = m;
        }
        break;
    default:
        assert(0);;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Return the current values used for PWM n, m:
//////////////////////////////////////////////////////////////////////

int
GPIO::get_pwm_ratio(int gpio,uint32_t& m,uint32_t& s) {
    int pwmx, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwmx,altf)) != 0 )
        return rc;                  // Return error

    switch ( pwmx ) {
    case 0:
        {
            u_PWM_RNG& regrng = PWMRNG(PWM_RNG1);
            u_PWM_DAT& regdat = PWMDAT(PWM_DAT1);

            s = regrng.u;
            m = regdat.u;
        }
        break;
    case 1:
        {
            u_PWM_RNG& regrng = PWMRNG(PWM_RNG2);
            u_PWM_DAT& regdat = PWMDAT(PWM_DAT2);

            s = regrng.u;
            m = regdat.u;
        }
        break;
    default:
        assert(0);;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Enable PWM on gpio (assumes PWM clock and device is configured)
//////////////////////////////////////////////////////////////////////

int
GPIO::pwm_enable(int gpio,bool enable) {
    u_PWM_CTL& regctl = PWMCTL(PWM_CTL);
    u_PWM_STA& regsta = PWMSTA(PWM_STA);
    int pwm, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwm,altf)) != 0 )
        return rc;                  // Return error

    switch ( pwm ) {
    case 0:
        if ( enable && regsta.s.GAPO1 )
            regsta.s.GAPO1 = 1;
        regctl.s.PWEN1 = enable ? 1 : 0;
        break;
    case 1:
        if ( enable && regsta.s.GAPO2 )
            regsta.s.GAPO2 = 1;
        regctl.s.PWEN2 = enable ? 1 : 0;
        break;
    default:
        assert(0);;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Write to PWM FIFO
//
// Note:
//  1. There is only one FIFO shared amongst up to two PWMs.
//  2. If multiple PWMs enabled, the data is interleaved.
//  3. Returns EIO if there is (a) no FIFO in effect, or
//     (b) FIFO has an uncleared error, (c) a BERR (bus error)
//     is in effect.
//  4. When the FIFO becomes full, or the entire buffer was
//     written, zero is returned. The returned # of words
//     actually written is passed thru argument n_words.
//  5. Use pwm_status() to determine status flags.
//////////////////////////////////////////////////////////////////////

int
GPIO::pwm_write_fifo(int gpio,uint32_t *data,size_t& n_words) {
    u_PWM_CTL& regctl = PWMCTL(PWM_CTL);
    u_PWM_STA& regsta = PWMSTA(PWM_STA);
    int pwm, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwm,altf)) != 0 )
        return rc;                  // Return error

    if ( !regctl.s.USEF1 && !regctl.s.USEF2 )
        return EIO;                 // No PWM accepting FIFO

    if ( regsta.s.WERR1 )
        return EIO;    

    if ( regsta.s.BERR )
        return EIO;

    u_PWM_FIFO& regfifo = PWMFIFO(PWM_FIF1);
    uint32_t count = 0;

    for ( ; count < n_words; ++count ) {
        regfifo.u = data[count];
        if ( regsta.s.WERR1 || regsta.s.BERR ) {
            n_words = count;        // Return count written out
            return EIO;
        } else if ( regsta.s.FULL1 )
            break;                  // FIFO can take no more
    }

    n_words = count;                // Return count written out
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Clear PWM status bit(s)
//
// Notes:
//  1.  Bits in status.fifo_werr, .fifo_rerr, .gap_occurred,
//      and .bus_error are used to determine which flags to clear.
//  2.  status.chan_state, .fifo_full, .fifo_empty are ignored;
//////////////////////////////////////////////////////////////////////

int
GPIO::pwm_clear_status(int gpio,const s_PWM_status& status) {
    u_PWM_STA& regsta = PWMSTA(PWM_STA);
    int pwmx, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwmx,altf)) != 0 )
        return rc;                  // Return error

    u_PWM_STA tmpsta;

    tmpsta.u = 0;
    tmpsta.s.FULL1 = status.fifo_full;
    tmpsta.s.RERR1 = status.fifo_rerr;
    tmpsta.s.WERR1 = status.fifo_werr;
    tmpsta.s.BERR = status.bus_error;

    switch ( pwmx ) {
    case 0 :
        tmpsta.s.GAPO1 = status.gap_occurred;
        break;
    case 1 :
        tmpsta.s.GAPO2 = status.gap_occurred;
        break;
    }

    regsta.u = tmpsta.u;

    return 0;
}

//////////////////////////////////////////////////////////////////////
// Return true if the FIFO is full
//////////////////////////////////////////////////////////////////////

bool
GPIO::pwm_fifo_full(int gpio) {
    int pwmx, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwmx,altf)) != 0 )
        abort();    // Not open / invalid gpio

    u_PWM_STA& regsta = PWMSTA(PWM_STA);
    return regsta.s.FULL1 ? true : false;
}

//////////////////////////////////////////////////////////////////////
// Return true if the FIFO is empty
//////////////////////////////////////////////////////////////////////

bool
GPIO::pwm_fifo_empty(int gpio) {
    int pwmx, rc;
    IO altf;

    if ( (rc = GPIO::pwm(gpio,pwmx,altf)) != 0 )
        abort();    // Not open / invalid gpio

    u_PWM_STA& regsta = PWMSTA(PWM_STA);
    return regsta.s.EMPT1 ? true : false;
}

//////////////////////////////////////////////////////////////////////
// Return the peripheral base address for the present platform
//////////////////////////////////////////////////////////////////////

uint32_t
GPIO::peripheral_base() {
    static uint32_t pbase = 0;
    int fd, rc;
    unsigned char buf[8];

    if ( !pbase ) { // Fetch first time only
        // Adjust for RPi2
        fd = ::open("/proc/device-tree/soc/ranges",O_RDONLY);
	if ( fd >= 0 ) {
	        rc = ::read(fd,buf,sizeof buf);
	        assert(rc==sizeof buf);
	        ::close(fd);
	        pbase = buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7] << 0;
	} else	{
		// Punt: Assume RPi2
		pbase = BCM2708_PERI_BASE;
	}
    }

    return pbase;
}

const char *
GPIO::source_name(Source src) {

    switch ( src ) {
    case Gnd:
        return "Gnd";
    case Oscillator:
        return "Oscillator";
    case PLLA:
        return "PLLA";
    case PLLC:
        return "PLLC";
    case PLLD:
        return "PLLD";
    case HDMI_Aux:
        return "HDMI_Aux";
    default:
        return "?";
    }
}

const char *
GPIO::alt_name(IO io) {

    switch ( io ) {
    case Input:
        return "Input";
    case Output:
        return "Output";
    case Alt0:
        return "Alt0";
    case Alt1:
        return "Alt1";
    case Alt2:
        return "Alt2";
    case Alt3:
        return "Alt3";
    case Alt4:
        return "Alt4";
    case Alt5:
        return "Alt5";
    default:
        return "?";
    }
}

const char *
GPIO::gpio_alt_func(int gpio,IO io) {

    if ( gpio < 0 || gpio > 31 )
        return "?";
    if ( int(io) < 0 || int(io) > 7 )
        return "?";

    if ( io == Input )
        return "Input";
    else if ( io == Output )
        return "Output";

    std::unordered_map<int,std::string>& map = *gpio_alts[io];
    const std::string& text = map[gpio];
    return text.c_str();
}

// End gpio.cpp
