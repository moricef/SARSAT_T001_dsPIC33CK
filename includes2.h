/* 
 * File:   newfile.h
 * Author: Fab2
 *
 * Created on July 24, 2025, 4:42 PM
 */

#ifndef NEWFILE_H
#define	NEWFILE_H

#include <xc.h>
#include <stdint.h>
#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif

// Configuration settings
#pragma config FNOSC = FRC          // Fast RC Oscillator
#pragma config FCKSM = CSECMD       // Clock switching enabled
#pragma config FWDTEN = 0           // Watchdog timer disabled
#pragma config POSCMD = NONE        // Primary oscillator disabled
#pragma config OSCIOFNC = ON        // OSC2 as digital I/O
#pragma config ICS = PGD1               // ICD Communication Channel Select bits
#pragma config JTAGEN = OFF             // JTAG Enable bit

// System clock frequency (100 MHz)
#define FCY 100000000UL


#ifdef	__cplusplus
}
#endif

#endif	/* NEWFILE_H */

