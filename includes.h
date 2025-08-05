#ifndef INCLUDES_H
#define	INCLUDES_H

// MUST DEFINE FAMILY BEFORE INCLUDING <xc.h>
#define __dsPIC33CK__

// Define frequencies
#define FOSC 100000000UL
#define FCY (FOSC / 2)
#define _XTAL_FREQ FOSC

#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libpic30.h>
#include <math.h>  // For GPS calculations
#include <stdbool.h>
// Debug UART Configuration
#define DEBUG_UART_TX_PIN  _RB9  // Curiosity Nano: RB9/CNANO2
#define DEBUG_BAUD_RATE    115200

#ifdef	__cplusplus
extern "C" {
#endif

// Clock Configuration
#pragma config FNOSC = FRC          // Fast RC Oscillator
#pragma config FCKSM = CSECMD       // Clock switching enabled
#pragma config FWDTEN = 0           // Watchdog timer disabled
#pragma config POSCMD = NONE        // Primary oscillator disabled
#pragma config OSCIOFNC = ON        // OSC2 as digital I/O
#pragma config ICS = PGD1           // ICD Communication Channel Select
#pragma config JTAGEN = OFF         // JTAG Enable bit

#ifdef	__cplusplus
}
#endif

#endif	/* INCLUDES_H */