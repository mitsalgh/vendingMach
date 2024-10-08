/**
 * Project: MateDealer
 * File name: uplink.c
 * Description:  Communication methods for interacting with the PC. 
 *   
 * @author bouni
 * @email bouni@owee.de  
 *   
 */

#ifndef F_CPU
#define F_CPU       16000000UL
#endif

#include <avr/io.h>
#include <inttypes.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "usart.h"
#include "mdb.h"
#include "uplink.h"

cmdStruct_t CMD_LIST[] = {
    {"reset",cmd_reset},
    {"help",cmd_help},
	{"info",cmd_info},
	{"vmc",cmd_vmc},
    {"expansion-enable", cmd_expansion_enable},
    {"mdb-state",cmd_get_mdb_state},
    {"start-session",cmd_start_session},
    {"approve-vend",cmd_approve_vend},
    {"deny-vend",cmd_deny_vend},
    {"cancel-session",cmd_cancel_session},
    /*
    {"read",cmd_read},
    {"write",cmd_write},*/
	{NULL,NULL} 
};

extern uint8_t mdb_state;
extern uint8_t mdb_poll_reply;
extern vmcCfg_t vmc;
extern vmcPrice_t price;
extern mdbSession_t session;
extern vmcDevice_t vmcDevice;
extern vmcExpansionEnableOptions_t vmcExpansionEnableOptions;

char buff[20];

//uint8_t eeByte EEMEM;

void uplink_cmd_handler(void) {
    
    static char cmd[20];
    static uint8_t index = 0;
    
    // No data received, return
    if(buffer_level(UPLINK_USART,RX) < 1) return;
    
    // flush cmd buffer if cmd is out of a valid length
    if(index == MAX_CMD_LENGTH) {
        index = 0;
    }

    // append char to cmd
    recv_char(UPLINK_USART, &cmd[index]);
    
    switch(cmd[index]) {
        case '\r':
            // carriage return received, replace with stringtermination and parse
            send_str(UPLINK_USART, "\r\n");
            cmd[index] = '\0';
            parse_cmd(cmd);
            index = 0;
            break;
        case '\n':
            // do nothing, but avoid index from incrementing
            break;
        case '\b':
            // backspace, remove last received char
            index--;
            send_char(UPLINK_USART, '\b');
            break;
            // char is part of an ESC sequence
        case 0x1B:
        case 0x5B:
            index++;
            break;
            // each other if the last two char was not part of an ESC sequence
        default:
            if(cmd[index - 1] == 0x5B && cmd[index - 2] == 0x1B) {
                    index = index - 2;
            } else {
                send_char(UPLINK_USART, cmd[index]);
                index++;
            }
    }
  
}

void parse_cmd(char *cmd) {
    
	char *tmp;
	uint8_t index = 0;
     
    // seperate command from arguments
    tmp = strsep(&cmd," "); 
    
    // search in command list for the command
	while(strcasecmp(CMD_LIST[index].cmd,tmp)) {
        if(CMD_LIST[index + 1].cmd == NULL) {
            send_str_p(UPLINK_USART,PSTR("Error: Unknown command\r\n"));    
            return;
        }
        index++;
    }

    // run the command
    CMD_LIST[index].funcptr(cmd);
	return;
  
}

void cmd_reset(char *arg) {
    RESET();
}

void cmd_help(char *arg) {
    send_str_p(UPLINK_USART, PSTR("-----------------------------------------------\r\n"));
    send_str_p(UPLINK_USART, PSTR("reset: reset the Arduino\r\n"));
    send_str_p(UPLINK_USART, PSTR("info: shows the VMC infos transfered during the setup process\r\n"));
    send_str_p(UPLINK_USART, PSTR("vmc: shows the VMC device info\r\n"));
    send_str_p(UPLINK_USART, PSTR("expansion-enable: shows the VMC device expansion enable info\r\n"));
    send_str_p(UPLINK_USART, PSTR("mdb-state: displays the current MDB state.\r\n"));
    send_str_p(UPLINK_USART, PSTR("start-session <funds>: starts a session with <funds> Euro Cents.\r\n"));
    send_str_p(UPLINK_USART, PSTR("approve-vend <vend-amount>: approves a vend request with <vend-amount> Euro Cents.\r\n"));
    send_str_p(UPLINK_USART, PSTR("deny-vend: denies a vend request.\r\n"));
    send_str_p(UPLINK_USART, PSTR("-----------------------------------------------\r\n"));
}

void cmd_info(char *arg) {
    // if(mdb_state >= MDB_ENABLED) {
        char buffer[40];
        send_str_p(UPLINK_USART, PSTR("-----------------------------------------------\r\n"));
        send_str_p(UPLINK_USART,PSTR("## VMC configuration data ##\r\n")); 
        sprintf(buffer,"VMC feature level:       %d\r\n", vmc.feature_level);
        send_str(UPLINK_USART,buffer);  
        sprintf(buffer,"VMC display columns:     %d\r\n", vmc.dispaly_cols);
        send_str(UPLINK_USART,buffer);  
        sprintf(buffer,"VMC display rows:        %d\r\n", vmc.dispaly_rows);
        send_str(UPLINK_USART,buffer);  
        sprintf(buffer,"VMC display info:        %d\r\n", vmc.dispaly_info);
        send_str(UPLINK_USART,buffer);

        send_str_p(UPLINK_USART,PSTR("##    VMC price range     ##\r\n"));
        uint32_t priceValue = price.maxLow | (price.max < 16);
        sprintf(buffer,"Maximum price:           %d\r\n", priceValue);
        send_str(UPLINK_USART,buffer);

        priceValue = price.minLow | (price.min < 16);
        sprintf(buffer,"Minimum price:           %d\r\n", priceValue);
        send_str(UPLINK_USART,buffer);

        sprintf(buffer,"currency:           %x\r\n", price.currency);
        send_str(UPLINK_USART,buffer);

        send_str_p(UPLINK_USART,PSTR("-----------------------------------------------\r\n"));  
    // } else {
    //     send_str_p(UPLINK_USART,PSTR("Error: Setup not yet completed!\r\n"));  
    // }
}

void cmd_vmc(char *arg) {
    char buffer[80];
    send_str_p(UPLINK_USART, PSTR("-----------------------------------------------\r\n"));
    send_str_p(UPLINK_USART,PSTR("## VMC device info ##\r\n"));

    sprintf(buffer,"manufacturerCode: %x %x %x\r\n",
        vmcDevice.manufacturerCode[0],
        vmcDevice.manufacturerCode[1],
        vmcDevice.manufacturerCode[2]
    );
    send_str(UPLINK_USART,buffer);

    sprintf(buffer,"serialNumber: %x %x %x %x %x %x %x %x %x %x %x %x\r\n",
        vmcDevice.serialNumber[0],
        vmcDevice.serialNumber[1],
        vmcDevice.serialNumber[2],
        vmcDevice.serialNumber[3],
        vmcDevice.serialNumber[4],
        vmcDevice.serialNumber[5],
        vmcDevice.serialNumber[6],
        vmcDevice.serialNumber[7],
        vmcDevice.serialNumber[8],
        vmcDevice.serialNumber[9],
        vmcDevice.serialNumber[10],
        vmcDevice.serialNumber[11]
    );
    send_str(UPLINK_USART,buffer);
    
    sprintf(buffer,"modelNumber: %x %x %x %x %x %x %x %x %x %x %x %x\r\n",
        vmcDevice.modelNumber[0],
        vmcDevice.modelNumber[1],
        vmcDevice.modelNumber[2],
        vmcDevice.modelNumber[3],
        vmcDevice.modelNumber[4],
        vmcDevice.modelNumber[5],
        vmcDevice.modelNumber[6],
        vmcDevice.modelNumber[7],
        vmcDevice.modelNumber[8],
        vmcDevice.modelNumber[9],
        vmcDevice.modelNumber[10],
        vmcDevice.modelNumber[11]
    );
    send_str(UPLINK_USART,buffer);

    sprintf(buffer,"softwareVersion: %x\r\n", vmcDevice.softwareVersion);
    send_str(UPLINK_USART,buffer);

    send_str_p(UPLINK_USART,PSTR("-----------------------------------------------\r\n"));
}

void cmd_expansion_enable(char *arg) {
    char buffer[40];
    send_str_p(UPLINK_USART, PSTR("-----------------------------------------------\r\n"));
    send_str_p(UPLINK_USART,PSTR("## VMC device expansion enable info ##\r\n"));
    sprintf(buffer,"enable: %x %x %x %x\r\n",
        vmcExpansionEnableOptions.cfg1,
        vmcExpansionEnableOptions.cfg2,
        vmcExpansionEnableOptions.cfg3,
        vmcExpansionEnableOptions.cfg4
    );
    send_str(UPLINK_USART,buffer);
}

void cmd_get_mdb_state(char *arg) {
    
    switch(mdb_state) {
        case MDB_INACTIVE:
            send_str_p(UPLINK_USART,PSTR("State: INACTIVE\r\n"));  
        break;
        case MDB_DISABLED:
            send_str_p(UPLINK_USART,PSTR("State: DISABLED\r\n"));  
        break;
        case MDB_ENABLED:
            send_str_p(UPLINK_USART,PSTR("State: ENABLED\r\n"));  
        break;
        case MDB_SESSION_IDLE:
            send_str_p(UPLINK_USART,PSTR("State: SESSION IDLE\r\n"));  
        break;
        case MDB_VENDING:
            send_str_p(UPLINK_USART,PSTR("State: VEND\r\n"));  
        break;
        case MDB_REVALUE:
            send_str_p(UPLINK_USART,PSTR("State: REVALUE\r\n"));  
        break;
        case MDB_NEGATIVE_VEND:
            send_str_p(UPLINK_USART,PSTR("State: NEGATIVE VEND\r\n"));  
        break;
    }
    
}

void cmd_start_session(char *arg) {
    //if(mdb_state == MDB_ENABLED) {
        if(session.start.flag == 0) {
            session.start.flag = 1;
            session.start.funds = atoi(arg);
            mdb_poll_reply = MDB_REPLY_BEGIN_SESSION;
        } else {
            send_str_p(UPLINK_USART,PSTR("Error: Session is already running\r\n"));  
        }
    //} else {
    //    send_str_p(UPLINK_USART,PSTR("Error: MateDealer not ready for a session\r\n"));  
    //}
}

void cmd_approve_vend(char *arg) {
    if(mdb_state == MDB_VENDING) {
        session.result.vend_approved = 1;
        session.result.vend_amount = atoi(arg);
        mdb_poll_reply = MDB_REPLY_VEND_APPROVED;
    } else {
        send_str_p(UPLINK_USART,PSTR("Error: MateDealer is not in a suitable state to approve a vend\r\n"));  
    }
}

void cmd_deny_vend(char *arg) {
    if(mdb_state == MDB_VENDING) {
        session.result.vend_denied = 1;
        mdb_poll_reply = MDB_REPLY_VEND_DENIED;
    } else {
        send_str_p(UPLINK_USART,PSTR("Error: MateDealer is not in a suitable state to deny a vend\r\n"));  
    }
}

void cmd_cancel_session(char *arg) {
    if(mdb_state == MDB_SESSION_IDLE) {
        mdb_poll_reply = MDB_REPLY_SESSION_CANCEL_REQ;
    } else {
        send_str_p(UPLINK_USART,PSTR("Error: MateDealer is not in a suitable state to cancel a session\r\n"));  
    }
}
