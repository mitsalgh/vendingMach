/**
 * Project: MateDealer
 * File name: main.c
 * Description:  Main file of the MateDealer Project
 *   
 * @author bouni
 * @email bouni@owee.de  
 *   
 */
 
#ifndef F_CPU
#define F_CPU       16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
// #include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "usart.h"
#include "mdb.h"
#include "uplink.h"

int main(void) {

    //setup_usart(2,38400,8,'N',1);
    //setup_usart(UPLINK_USART, 57600, 8, 'N', 1);
    setup_usart(UPLINK_USART, 256000, 8, 'N', 1);
    setup_usart(MDB_USART, 9600, 9, 'N', 1);
    
    sei();
    
    send_str_p(UPLINK_USART,PSTR("FTP Cashless is up and running\r\n===========\r\n"));
       
    // Main Loop
    while(1) {
        mdb_cmd_handler();
        uplink_cmd_handler();
        // forward_to_pc();
        // forward_to_mdb();
    }
    
    return 0;
}
