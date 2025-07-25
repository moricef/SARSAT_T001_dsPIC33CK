/* 
 * File:   includes.h
 * Author: Fab2
 *
 * Created on July 24, 2025, 8:28 PM
 */

#ifndef INCLUDES_H
#define	INCLUDES_H

// MUST DEFINE BEFORE INCLUDING <xc.h>
#define FOSC 100000000UL            // System clock frequency (Hz)
#define FCY (FOSC / 2)              // Instruction cycle frequency (50 MHz)
#define _XTAL_FREQ FOSC             // Required for __delay_us()

#include <xc.h>  // Now included after _XTAL_FREQ definition
#include <stdint.h>
#include <string.h>
#include <libpic30.h>

#ifdef	__cplusplus
extern "C" {
#endif

// Configuration FRC + PLL pour 100 MHz
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

#endif	/* INCLUDES3_H */