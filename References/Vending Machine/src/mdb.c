/**
* Project: MateDealer
* File name: mdb.c
* Description:  MultiDropBus methods.
*               See www.vending.org/technology/MDB_Version_4-2.pdf for Protocoll information.
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
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <stdio.h>
#include "usart.h"
#include "uplink.h"
#include "mdb.h"

uint8_t mdb_state = MDB_INACTIVE;
uint8_t mdb_poll_reply = MDB_REPLY_ACK;
uint8_t mdb_active_cmd = MDB_IDLE;

uint8_t reset_done = FALSE;

uint8_t mdb_expansion_sub_command = 0;

extern volatile uint8_t cmd_var[MAX_VAR];

vmcCfg_t vmc = {0,0,0,0};
vmcPrice_t price = {0,0,0,0,0};

cdCfg_t cd = {
	0x01,   // Reader CFG (constant)
	0x03,   // Feature Level [1,2,3]
	0x1360, // Country Code
	0x64,   // Scale Factor
	0x02,   // Decimal Places
	0x05,   // max Response Time
	0x00    // Misc Options
};

mdbSession_t session = {
	{0,0},
	{0,0,0}
};

vmcDevice_t vmcDevice = {
	{0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0},
	0
};

vmcExpansionEnableOptions_t vmcExpansionEnableOptions = {
	0,0,0,0
};

uint8_t mdb_setup_stage = MDB_SETUP_STATE_IDLE;
uint8_t mdb_setup_config_data_offset = 0;
uint8_t mdb_setup_config_data_buffer[5];
uint8_t mdb_setup_min_max_level_offset = 0;
uint8_t mdb_setup_min_max_level_1_2_buffer[5];
uint8_t mdb_setup_min_max_level_3_buffer[12];
uint8_t mdb_setup_min_max_level_3_max = 0;

void forward_to_pc(void) {
	if (buffer_level(MDB_USART, RX) < 1)
		return;
	uint8_t data = recv_byte(MDB_USART);
	send_byte(UPLINK_USART, data);
}

void forward_to_mdb(void) {
	if (buffer_level(UPLINK_USART, RX) < 1)
		return;
	uint8_t data = recv_byte(UPLINK_USART);
	send_byte(MDB_USART, data);
}

void mdb_cmd_handler(void) {
	char buffer[40];
	switch(mdb_active_cmd) {
		
		case MDB_IDLE:
			// Wait for enough data in buffer
			if(buffer_level(MDB_USART,RX) < 2) return;
			// read data from buffer
			uint16_t data = recv_mdb(MDB_USART);
		
			if (data == 0x000) {
				send_str_p(UPLINK_USART,PSTR("ACK 0x000\r\n"));
				break;
			}
			else if (data == 0x0AA) {
				// RET
				send_str_p(UPLINK_USART,PSTR("RET 0x0AA\r\n"));
				break;
			}
			else if (data == 0x0FF) {
				// NAK
				send_str_p(UPLINK_USART,PSTR("NAK 0x0FF\r\n"));
				break;
			}
			// if modebit is set and command is in command range for cashless device
			if((data & 0x100) == 0x100) {
				data = data & 0xFF;
				if (data == 0x00) {
					// ACK
					send_str_p(UPLINK_USART,PSTR("ACK 0x100\r\n"));
				}
				else if (data == 0xAA) {
					// RET
					send_str_p(UPLINK_USART,PSTR("RET 0x1AA\r\n"));
				}
				else if (data == 0xFF) {
					// NAK
					send_str_p(UPLINK_USART,PSTR("NAK 0x1FF\r\n"));
				}
				else if ((data >= MDB_RESET) && (data <= MDB_EXPANSION)) {
					//Set command as active command
					mdb_active_cmd = data;
					if(!reset_done && mdb_active_cmd != MDB_RESET) {
						mdb_active_cmd = MDB_IDLE;
					}
				}
			}
		
			//if ((data & 0x100) == 0x100) {
				//uint8_t addr = data & 0xff;
				//if ((addr >= MDB_RESET) && (addr <= MDB_EXPANSION)) {
					////sprintf(buffer, "mdb_active_cmd %x\r\n", addr);
					////send_str(UPLINK_USART, buffer);
					//mdb_active_cmd = addr;
					////if(!reset_done && mdb_active_cmd != MDB_RESET) {
					////mdb_active_cmd = MDB_IDLE;
					////}
				//}
			//}
			break;
		
		case MDB_RESET:
			//send_str_p(UPLINK_USART,PSTR("MDB_RESET\r\n"));
			mdb_reset();
			break;
		
		case MDB_SETUP:
			//send_str_p(UPLINK_USART,PSTR("MDB_SETUP\r\n"));
			mdb_setup();
			break;
		
		case MDB_POLL:
			//send_str_p(UPLINK_USART,PSTR("MDB_POLL\r\n"));
			// mdb_poll();
			mdb_poll_2();
			break;
		
		case MDB_VEND:
			//send_str_p(UPLINK_USART,PSTR("MDB_VEND\r\n"));
			mdb_vend();
			break;
		
		case MDB_READER:
			//send_str_p(UPLINK_USART,PSTR("MDB_READER\r\n"));
			mdb_reader2();
			break;
		
		case MDB_REVALUE_ADDR:
			send_str_p(UPLINK_USART,PSTR("MDB_REVALUE_ADDR\r\n"));
			break;
		
		case MDB_EXPANSION:
			//send_str_p(UPLINK_USART,PSTR("MDB_EXPANSION\r\n"));
			mdb_expansion();
			//mdb_active_cmd = MDB_TEST;
			break;
		
		case MDB_EXPANSION_REQUEST_ID:
			ExpansionRequestID();
			break;

		case MDB_EXPANSION_ENABLE_OPTIONS:
			ExpansionEnableOptions();
			break;

		case MDB_READER_PROCESS:
			mdb_reader_process();
			break;
			
		case MDB_TEST:
			MdbTest();
			break;
		
		default:
		sprintf(buffer, "mdb_active_cmd %x\r\n", mdb_active_cmd);
		send_str(UPLINK_USART,buffer);
		break;
	}
}

void MdbTest(void) {
	uint8_t level = buffer_level(MDB_USART,RX);
	if (level < 2) return;
	
	uint16_t data = recv_mdb(MDB_USART);
	char buffer[30];
	sprintf(buffer, "MdbTest other %x %d\r\n", data, level);
	send_str(UPLINK_USART,buffer);
}

void mdb_reset(void) {
	
	// Wait for enough data in buffer to proceed reset
	if(buffer_level(MDB_USART,RX) < 2) return;

	#if DEBUG == 1
	//send_str_p(UPLINK_USART, PSTR("RESET\r\n"));
	#endif
	
	// validate checksum
	if(recv_mdb(MDB_USART) != MDB_RESET) {
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		send_str_p(UPLINK_USART,PSTR("Error: invalid checksum for [RESET]\r\n"));
		return;
	}

	// Reset everything
	vmc.feature_level = 0;
	vmc.dispaly_cols = 0;
	vmc.dispaly_rows = 0;
	vmc.dispaly_info = 0;
	price.max = 0;
	price.min = 0;
	price.maxLow = 0;
	price.minLow = 0;
	price.currency = 0;

	uint8_t t = 0;
	for (t = 0; t < 3; t++)
		vmcDevice.manufacturerCode[t] = 0;
	for (t = 0; t < 12; t++)
		vmcDevice.serialNumber[t] = 0;
	for (t = 0; t < 12; t++)
		vmcDevice.modelNumber[t] = 0;
	vmcDevice.softwareVersion = 0;

	vmcExpansionEnableOptions.cfg1 = 0;
	vmcExpansionEnableOptions.cfg2 = 0;
	vmcExpansionEnableOptions.cfg3 = 0;
	vmcExpansionEnableOptions.cfg4 = 0;

	// Send ACK
	send_mdb(MDB_USART, 0x100);
	reset_done = TRUE;
	mdb_state = MDB_INACTIVE;
	mdb_active_cmd = MDB_IDLE;
	mdb_poll_reply = MDB_REPLY_JUST_RESET;
	mdb_setup_stage = MDB_SETUP_STATE_IDLE;
}

void mdb_setup(void) {
	switch (mdb_setup_stage)
	{
	case MDB_SETUP_STATE_IDLE:
		if (buffer_level(MDB_USART,RX) < 2)
			return;
		uint8_t mdb_setup_state = (uint8_t) recv_mdb(MDB_USART);
		if (mdb_setup_state == 0) {
			mdb_setup_config_data_offset = 0;
			mdb_setup_stage = MDB_SETUP_STATE_CONFIG_DATA;
		}
		else if (mdb_setup_state == 1) {
			mdb_setup_min_max_level_offset = 0;
			mdb_setup_min_max_level_3_max = 5;
			mdb_setup_stage = MDB_SETUP_STATE_MIN_MAX;
		}
		break;
	case MDB_SETUP_STATE_CONFIG_DATA:
		mdb_setup_config_data();
		break;
	case MDB_SETUP_STATE_MIN_MAX:
		if ((vmc.feature_level == 1) || (vmc.feature_level == 2))
			mdb_setup_min_max_level_1_2();
		else if (vmc.feature_level == 3)
			mdb_setup_min_max_level_3();
		else {
			mdb_active_cmd = MDB_IDLE;
			mdb_poll_reply = MDB_REPLY_ACK;
			send_str_p(UPLINK_USART, PSTR("Error: unsupported vmc level subcommand [SETUP]\r\n"));
		}
		break;
	default:
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		send_str_p(UPLINK_USART, PSTR("Error: unknown subcommand [SETUP]\r\n"));
		break;
	}
}

void mdb_setup_config_data(void) {
	if (buffer_level(MDB_USART,RX) < 2)
		return;
	uint8_t data = (uint8_t) recv_mdb(MDB_USART);
	mdb_setup_config_data_buffer[mdb_setup_config_data_offset] = data;
	mdb_setup_config_data_offset++;
	if (mdb_setup_config_data_offset < 5)
		return;
	mdb_setup_stage = MDB_SETUP_STATE_IDLE;
	char buffer[80];

	uint16_t checksum = 0x100 + MDB_SETUP + 0;
	checksum += mdb_setup_config_data_buffer[0] + 
		mdb_setup_config_data_buffer[1] + 
		mdb_setup_config_data_buffer[2] + 
		mdb_setup_config_data_buffer[3];
	checksum = checksum & 0xFF;
	uint16_t verifyChecksum = mdb_setup_config_data_buffer[4];

	if (checksum != verifyChecksum) {
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		sprintf(buffer, "SETUP: invalid checksum %x %x\r\n", checksum, verifyChecksum);
		send_str(UPLINK_USART, buffer);
		return;
	}

	vmc.feature_level = mdb_setup_config_data_buffer[0];
	vmc.dispaly_cols  = mdb_setup_config_data_buffer[1];
	vmc.dispaly_rows  = mdb_setup_config_data_buffer[2];
	vmc.dispaly_info  = mdb_setup_config_data_buffer[3];

	sprintf(buffer, "SETUP: vmc %d %d %d %d\r\n", 
		vmc.feature_level, vmc.dispaly_cols, vmc.dispaly_rows, vmc.dispaly_info);
	send_str(UPLINK_USART, buffer);

	checksum = ((cd.reader_cfg +
		cd.feature_level +
		(cd.country_code >> 8) +
		(cd.country_code & 0xFF) +
		cd.scale_factor +
		cd.decimal_places +
		cd.max_resp_time +
		cd.misc_options) & 0xFF) | 0x100;

	// Send own config data
	send_mdb(MDB_USART, cd.reader_cfg);
	send_mdb(MDB_USART, cd.feature_level);
	send_mdb(MDB_USART, (cd.country_code >> 8));
	send_mdb(MDB_USART, (cd.country_code & 0xFF));
	send_mdb(MDB_USART, cd.scale_factor);
	send_mdb(MDB_USART, cd.decimal_places);
	send_mdb(MDB_USART, cd.max_resp_time);
	send_mdb(MDB_USART, cd.misc_options);
	send_mdb(MDB_USART, checksum);

	// Set MDB State
	mdb_state = MDB_DISABLED;
	
	mdb_active_cmd = MDB_IDLE;
	mdb_poll_reply = MDB_REPLY_READER_CFG;
}

void mdb_setup_min_max_level_1_2(void) {
	if (buffer_level(MDB_USART,RX) < 2)
		return;
	uint8_t data = (uint8_t) recv_mdb(MDB_USART);
	mdb_setup_min_max_level_1_2_buffer[mdb_setup_min_max_level_offset] = data;
	mdb_setup_min_max_level_offset++;
	if (mdb_setup_min_max_level_offset < 5)
		return;
	mdb_setup_stage = MDB_SETUP_STATE_IDLE;
	char buffer[80];

	uint16_t checksum = 0x100 + MDB_SETUP + 1;
	// checksum += mdb_setup_min_max_level_1_2_buffer[0] + 
	// 	mdb_setup_min_max_level_1_2_buffer[1] + 
	// 	mdb_setup_min_max_level_1_2_buffer[2] + 
	// 	mdb_setup_min_max_level_1_2_buffer[3];
	checksum += calc_checksum(mdb_setup_min_max_level_1_2_buffer, 4);
	checksum = checksum & 0xFF;
	uint16_t verifyChecksum = mdb_setup_min_max_level_1_2_buffer[4];

	if (checksum != verifyChecksum) {
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		sprintf(buffer, "SETUP: invalid checksum level 1 or 2 %x %x\r\n", checksum, verifyChecksum);
		send_str(UPLINK_USART, buffer);
		return;
	}
	price.max = (mdb_setup_min_max_level_1_2_buffer[0] << 8) | mdb_setup_min_max_level_1_2_buffer[1];
	price.min = (mdb_setup_min_max_level_1_2_buffer[2] << 8) | mdb_setup_min_max_level_1_2_buffer[3];

	// send ACK
	send_mdb(MDB_USART, 0x100);

	// Set MDB State
	mdb_state = MDB_DISABLED;

	mdb_active_cmd = MDB_IDLE;
	mdb_poll_reply = MDB_REPLY_ACK;
	send_str_p(UPLINK_USART,PSTR("mdb_setup_min_max_level_1_2\r\n"));
}

void mdb_setup_min_max_level_3(void) {
	// send_str_p(UPLINK_USART, PSTR("SETUP STAGE mdb_setup_min_max_level_3\r\n"));
	if (buffer_level(MDB_USART,RX) < 2)
		return;
	uint8_t data = (uint8_t) recv_mdb(MDB_USART);
	mdb_setup_min_max_level_3_buffer[mdb_setup_min_max_level_offset] = data;
	mdb_setup_min_max_level_offset++;
	if (mdb_setup_min_max_level_offset != mdb_setup_min_max_level_3_max)
		return;
	char buffer[80];

	uint16_t checksum =  0x100 + MDB_SETUP + 0x01;
	/*
	for (uint8_t t = 0; t < mdb_setup_min_max_level_3_max; t++) {
		checksum += mdb_setup_min_max_level_3_buffer[t];
		// sprintf(buffer, "SETUP: checksum %d %x\r\n", t, mdb_setup_min_max_level_3_buffer[t]);
		// send_str(UPLINK_USART, buffer);
	}
	*/
	checksum += calc_checksum(mdb_setup_min_max_level_3_buffer, mdb_setup_min_max_level_3_max - 1);

	checksum = checksum & 0xFF;
	uint16_t verifyChecksum = mdb_setup_min_max_level_3_buffer[mdb_setup_min_max_level_3_max - 1];

	if (checksum != verifyChecksum) {
		if (mdb_setup_min_max_level_3_max == 5) {
			mdb_setup_min_max_level_3_max = 11;
			return;
		}
		mdb_setup_stage = MDB_SETUP_STATE_IDLE;
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		sprintf(buffer, "MINMAX: invalid_checksum_level_3 %x %x %d\r\n",
			checksum, verifyChecksum, mdb_setup_min_max_level_3_max
		);
		send_str(UPLINK_USART, buffer);

		for (uint8_t t = 0; t < mdb_setup_min_max_level_3_max; t++) {
			sprintf(buffer, "MINMAX: checksum %d %x\r\n", t, mdb_setup_min_max_level_3_buffer[t]);
			send_str(UPLINK_USART, buffer);
		}
		return;
	}
	mdb_setup_stage = MDB_SETUP_STATE_IDLE;
	if (mdb_setup_min_max_level_3_max == 5) {
		price.max = (mdb_setup_min_max_level_3_buffer[0] << 8) | mdb_setup_min_max_level_3_buffer[1];
		price.min = (mdb_setup_min_max_level_3_buffer[2] << 8) | mdb_setup_min_max_level_3_buffer[3];
	}
	else  {
		price.max = (mdb_setup_min_max_level_3_buffer[0] << 8) | mdb_setup_min_max_level_3_buffer[1];
		price.maxLow = (mdb_setup_min_max_level_3_buffer[2] << 8) | mdb_setup_min_max_level_3_buffer[3];

		price.min = (mdb_setup_min_max_level_3_buffer[4] << 8) | mdb_setup_min_max_level_3_buffer[5];
		price.minLow = (mdb_setup_min_max_level_3_buffer[6] << 8) | mdb_setup_min_max_level_3_buffer[7];
		
		price.currency = (mdb_setup_min_max_level_3_buffer[8] << 8) | mdb_setup_min_max_level_3_buffer[9];
	}

	// send ACK
	send_mdb(MDB_USART, 0x100);

	// Set MDB State
	mdb_state = MDB_DISABLED;

	mdb_active_cmd = MDB_IDLE;
	mdb_poll_reply = MDB_REPLY_ACK;
	
	sprintf(buffer, "mdb_setup_min_max_level_3_OK. size %d\r\n", mdb_setup_min_max_level_3_max);
	send_str(UPLINK_USART, buffer);
}

void mdb_send_config_reader(void) {
	// calculate checksum for own configuration
	uint16_t checksum = ((cd.reader_cfg +
		cd.feature_level +
		(cd.country_code >> 8) +
		(cd.country_code & 0xFF) +
		cd.scale_factor +
		cd.decimal_places +
		cd.max_resp_time +
		cd.misc_options) & 0xFF) | 0x100;

	// Send own config data
	send_mdb(MDB_USART, cd.reader_cfg);
	send_mdb(MDB_USART, cd.feature_level);
	send_mdb(MDB_USART, (cd.country_code >> 8));
	send_mdb(MDB_USART, (cd.country_code & 0xFF));
	send_mdb(MDB_USART, cd.scale_factor);
	send_mdb(MDB_USART, cd.decimal_places);
	send_mdb(MDB_USART, cd.max_resp_time);
	send_mdb(MDB_USART, cd.misc_options);
	send_mdb(MDB_USART, checksum);
}

void mdb_poll_2(void) {
	char buffer[40];
	if(buffer_level(MDB_USART,RX) < 2) return;
	

	if (recv_mdb(MDB_USART) != MDB_POLL) {
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		send_str_p(UPLINK_USART, PSTR("Error: Invalid checksum [Poll]\r\n"));
		return;
	}

	uint8_t command = mdb_poll_reply;
	mdb_poll_reply = MDB_REPLY_ACK;
	mdb_active_cmd = MDB_IDLE;

	uint16_t paymentData = 0x0000;
	uint8_t typeOfPayment = 0b10000000;

	switch (command)
	{
	case MDB_REPLY_ACK:
		send_mdb(MDB_USART, 0x100);
		// send_str_p(UPLINK_USART,PSTR("POLL OK\r\n"));
		break;
	case MDB_REPLY_JUST_RESET:
		send_mdb(MDB_USART, 0x000);
		send_mdb(MDB_USART, 0x100);
		send_str_p(UPLINK_USART, PSTR("MDB_REPLY_JUST_RESET [Poll]\r\n"));
		break;
	case MDB_REPLY_READER_CFG:
		mdb_send_config_reader();
		send_str_p(UPLINK_USART, PSTR("MDB_REPLY_READER_CFG [Poll]\r\n"));
		break;
	case MDB_REPLY_PERIPHERIAL_ID:
		SendPeripheralID();
		send_str_p(UPLINK_USART, PSTR("MDB_REPLY_PERIPHERIAL_ID [Poll]\r\n"));
		break;
	case MDB_REPLY_BEGIN_SESSION:
		//if(session.start.flag && state == 1) {

		send_mdb(MDB_USART, 0x003);

		// Funds Available – scaled
		send_mdb(MDB_USART, (session.start.funds >> 8));
		send_mdb(MDB_USART, (session.start.funds & 0xFF));
		
		// Payment media ID
		// send_mdb(MDB_USART, 0xFF);
		// send_mdb(MDB_USART, 0xFF);
		// send_mdb(MDB_USART, 0xFF);
		// send_mdb(MDB_USART, 0xFF);

		// Type of payment
		// send_mdb(MDB_USART, typeOfPayment);

		// Payment data
		// send_mdb(MDB_USART, (paymentData >> 8));
		// send_mdb(MDB_USART, (paymentData & 0xFF));

		uint16_t checksum = 0x003 + 
			(session.start.funds >> 8) + (session.start.funds & 0xFF)
			// 0xFF + 0xFF + 0xFF + 0xFF +
			// typeOfPayment + 
			// (paymentData >> 8) + (paymentData & 0xFF)
			;

		checksum = (checksum & 0xFF) | 0x100;
		send_mdb(MDB_USART, checksum);
			//state = 2;
		//}
		/*
		else if(session.start.flag && state == 2) {
			// wait for enough data in Buffer
			if(buffer_level(MDB_USART,RX) < 2) return; 
			// check if VMC sent ACK
			if(recv_mdb(MDB_USART) != 0x000) {
				mdb_active_cmd = MDB_IDLE;
				mdb_poll_reply = MDB_REPLY_ACK;
				session.start.flag = 0;
				session.start.funds = 0;
				state = 0;
				send_str_p(UPLINK_USART,PSTR("Error: no ACK received on [START SESSION]\r\n"));
				return;    
			}
			session.start.flag = 0;
			session.start.funds = 0;
			mdb_state = MDB_SESSION_IDLE;
			mdb_active_cmd = MDB_IDLE;
			mdb_poll_reply = MDB_REPLY_ACK;
			state = 0;
			return;
		}
		*/
		break;
	default:
		sprintf(buffer, "POLL: %x\r\n", command);
		send_str(UPLINK_USART, buffer);
		break;
	}
}

void mdb_poll(void) {
    
    static uint8_t state = 0;
    uint16_t checksum = 0;
    
    if(state == 0) {
        // Wait for enough data in buffer
        if(buffer_level(MDB_USART,RX) < 2) return; 
        
        #if DEBUG == 1
        // send_str_p(UPLINK_USART, PSTR("POLL\r\n"));
        #endif
        
        // validate checksum
        if(recv_mdb(MDB_USART) != MDB_POLL) {
            mdb_active_cmd = MDB_IDLE;
            mdb_poll_reply = MDB_REPLY_ACK;
            state = 0;
            send_str_p(UPLINK_USART,PSTR("Error: Invalid checksum [Poll]\r\n"));
            return;  
        } 
        state = 1;
    } 

    switch(mdb_poll_reply) {
        
        case MDB_REPLY_ACK:
            // send ACK
            send_mdb(MDB_USART, 0x100);
            mdb_active_cmd = MDB_IDLE;
            mdb_poll_reply = MDB_REPLY_ACK;
            state = 0;
        break;

        case MDB_REPLY_JUST_RESET:
            // send JUST RESET
            if(state == 1) {
                send_mdb(MDB_USART, 0x000);
                send_mdb(MDB_USART, 0x100);
                state = 2;
				#if DEBUG == 1
				send_str_p(UPLINK_USART, PSTR("POLL MDB_REPLY_JUST_RESET\r\n"));
				#endif
            }
            // wait for the ACK
            else if(state == 2) {
                // wait for enough data in Buffer
                if(buffer_level(MDB_USART,RX) < 2) return; 
                // check if VMC sent ACK
                if(recv_mdb(MDB_USART) != 0x000) {
                    mdb_active_cmd = MDB_IDLE;
                    mdb_poll_reply = MDB_REPLY_ACK;
                    state = 0;
                    send_str_p(UPLINK_USART,PSTR("Error: no ACK received on [JUST RESET]\r\n"));
                    return;    
                }

                mdb_active_cmd = MDB_IDLE;
                mdb_poll_reply = MDB_REPLY_ACK;
                state = 0;
                return;
            }
        break;
    
        case MDB_REPLY_READER_CFG:
            #if DEBUG == 1
			send_str_p(UPLINK_USART, PSTR("POLL MDB_REPLY_READER_CFG\r\n"));
			#endif
        break;

        case MDB_REPLY_DISPLAY_REQ:
            #if DEBUG == 1
			send_str_p(UPLINK_USART, PSTR("POLL MDB_REPLY_DISPLAY_REQ\r\n"));
			#endif		
        break;

        case MDB_REPLY_BEGIN_SESSION:
            if(session.start.flag && state == 1) {
                send_mdb(MDB_USART, 0x003);
                send_mdb(MDB_USART, (session.start.funds >> 8));
                send_mdb(MDB_USART, (session.start.funds & 0xFF));
                checksum = 0x003 + (session.start.funds >> 8) + (session.start.funds & 0xFF);
                checksum = (checksum & 0xFF) | 0x100;
                send_mdb(MDB_USART, checksum);
                state = 2;
            }
            
            else if(session.start.flag && state == 2) {
                // wait for enough data in Buffer
                if(buffer_level(MDB_USART,RX) < 2) return; 
                // check if VMC sent ACK
                if(recv_mdb(MDB_USART) != 0x000) {
                    mdb_active_cmd = MDB_IDLE;
                    mdb_poll_reply = MDB_REPLY_ACK;
                    session.start.flag = 0;
                    session.start.funds = 0;
                    state = 0;
                    send_str_p(UPLINK_USART,PSTR("Error: no ACK received on [START SESSION]\r\n"));
                    return;    
                }
                session.start.flag = 0;
                session.start.funds = 0;
                mdb_state = MDB_SESSION_IDLE;
                mdb_active_cmd = MDB_IDLE;
                mdb_poll_reply = MDB_REPLY_ACK;
                state = 0;
                return;
            }
        break;

        case MDB_REPLY_SESSION_CANCEL_REQ:
            if(state == 1) {
                send_mdb(MDB_USART, 0x004);
                send_mdb(MDB_USART, 0x104);
                state = 2;
            }
            else if(state == 2) {
                // wait for enough data in Buffer
                if(buffer_level(MDB_USART,RX) < 2) return; 
                // check if VMC sent ACK
                if(recv_mdb(MDB_USART) != 0x000) {
                    mdb_active_cmd = MDB_IDLE;
                    mdb_poll_reply = MDB_REPLY_ACK;
                    session.start.flag = 0;
                    session.start.funds = 0;
                    state = 0;
                    send_str_p(UPLINK_USART,PSTR("Error: no ACK received on [SESSION CANCEL REQ]\r\n"));
                    return;    
                }
                session.start.flag = 0;
                session.start.funds = 0;
                mdb_active_cmd = MDB_IDLE;
                mdb_poll_reply = MDB_REPLY_ACK;
                state = 0;
                return;    
            }
        break;
        
        case MDB_REPLY_VEND_APPROVED:
            if(session.result.vend_approved && state == 1) {
                send_mdb(MDB_USART, 0x005);
                send_mdb(MDB_USART, (session.result.vend_amount >> 8));
                send_mdb(MDB_USART, (session.result.vend_amount & 0xFF));
                checksum = 0x005 + (session.result.vend_amount >> 8) + (session.result.vend_amount & 0xFF);
                checksum = (checksum & 0xFF) | 0x100;
                send_mdb(MDB_USART, checksum);
                state = 2;
            }
            else if(session.result.vend_approved && state == 2) {
                // wait for enough data in Buffer
                if(buffer_level(MDB_USART,RX) < 2) return; 
                // check if VMC sent ACK
                if(recv_mdb(MDB_USART) != 0x000) {
                    mdb_active_cmd = MDB_IDLE;
                    mdb_poll_reply = MDB_REPLY_ACK;
                    session.result.vend_approved = 0;
                    session.result.vend_amount = 0;
                    state = 0;
                    send_str_p(UPLINK_USART,PSTR("Error: no ACK received on [VEND APPROVE]\r\n"));
                    return;    
                }
                session.result.vend_approved = 0;
                session.result.vend_amount = 0;
                mdb_active_cmd = MDB_IDLE;
                mdb_poll_reply = MDB_REPLY_ACK;
                state = 0;
                return;    
            }
        break;
        
        case MDB_REPLY_VEND_DENIED:
            if(session.result.vend_denied && state == 1) {
                send_mdb(MDB_USART, 0x006);
                send_mdb(MDB_USART, 0x106);
                state = 2;
            }
            else if(session.result.vend_denied && state == 2) {
                // wait for enough data in Buffer
                if(buffer_level(MDB_USART,RX) < 2) return; 
                // check if VMC sent ACK
                if(recv_mdb(MDB_USART) != 0x000) {
                    mdb_active_cmd = MDB_IDLE;
                    mdb_poll_reply = MDB_REPLY_ACK;
                    session.start.flag = 0;
                    session.start.funds = 0;
                    session.result.vend_denied = 0;
                    state = 0;
                    send_str_p(UPLINK_USART,PSTR("Error: no ACK received on [VEND DENY]\r\n"));
                    return;    
                }
                session.start.flag = 0;
                session.start.funds = 0;
                session.result.vend_denied = 0;
                mdb_active_cmd = MDB_IDLE;
                mdb_poll_reply = MDB_REPLY_ACK;
                state = 0;
                return;    
            }
        break;
        
        case MDB_REPLY_END_SESSION:
            if(state == 1) {
                send_mdb(MDB_USART, 0x007);
                send_mdb(MDB_USART, 0x107);
                state = 2;
            }
            else if(state == 2) {
                // wait for enough data in Buffer
                if(buffer_level(MDB_USART,RX) < 2) return; 
                // check if VMC sent ACK
                if(recv_mdb(MDB_USART) != 0x000) {
                    mdb_active_cmd = MDB_IDLE;
                    mdb_poll_reply = MDB_REPLY_ACK;
                    state = 0;
                    send_str_p(UPLINK_USART,PSTR("Error: no ACK received on [END SESSION]\r\n"));
                    return;    
                }
                mdb_active_cmd = MDB_IDLE;
                mdb_poll_reply = MDB_REPLY_ACK;
                state = 0;
                return;    
            }
        break;
        
        case MDB_REPLY_CANCELED:
            if(state == 1) {
                send_mdb(MDB_USART, 0x008);
                send_mdb(MDB_USART, 0x108);
                state = 2;
            }
            else if(state == 2) {
                // wait for enough data in Buffer
                if(buffer_level(MDB_USART,RX) < 2) return; 
                // check if VMC sent ACK
                if(recv_mdb(MDB_USART) != 0x000) {
                    mdb_active_cmd = MDB_IDLE;
                    mdb_poll_reply = MDB_REPLY_ACK;
                    state = 0;
                    send_str_p(UPLINK_USART,PSTR("Error: no ACK received on [REPLY CANCELED]\r\n"));
                    return;    
                }
                mdb_active_cmd = MDB_IDLE;
                mdb_poll_reply = MDB_REPLY_ACK;
                state = 0;
                return;    
            }
        break;
        
        case MDB_REPLY_PERIPHERIAL_ID:
            #if DEBUG == 1
			send_str_p(UPLINK_USART, PSTR("POLL MDB_REPLY_PERIPHERIAL_ID\r\n"));
			#endif		
        break;
        
        case MDB_REPLY_ERROR:
            #if DEBUG == 1
			send_str_p(UPLINK_USART, PSTR("POLL MDB_REPLY_ERROR\r\n"));
			#endif		
        break;
        
        case MDB_REPLY_CMD_OUT_SEQUENCE:
            #if DEBUG == 1
			send_str_p(UPLINK_USART, PSTR("POLL MDB_REPLY_CMD_OUT_SEQUENCE\r\n"));
			#endif		
        break;
        
    }
}

void mdb_vend(void) {
	
	static uint8_t data[6] = {0,0,0,0,0,0};
	static uint8_t state = 0;
	uint8_t checksum = MDB_VEND;
	char buffer[40];
	
	// wait for the subcommand
	if(state == 0) {
		// wait for enough data in buffer
		if(buffer_level(MDB_USART,RX) < 2) return;
		
		// fetch the subommand from Buffer
		data[0] = recv_mdb(MDB_USART);
		state = 1;
	}
	
	// switch through subcommands
	switch(data[0]) {
		// vend request
		case 0:
		// wait for enough data in buffer
		if(buffer_level(MDB_USART,RX) < 10) return;

		#if DEBUG == 1
		send_str_p(UPLINK_USART, PSTR("VEND REQUEST\r\n"));
		#endif
		
		// fetch the data from buffer
		for(uint8_t i=1; i < 6; i++) {
			data[i] = (uint8_t) recv_mdb(MDB_USART);
		}
		
		// calculate checksum
		checksum += data[0] + data[1] + data[2] + data[3] + data[4];
		checksum &= 0xFF;
		
		// validate checksum
		if(checksum != data[5]) {
			state = 0;
			mdb_active_cmd = MDB_IDLE;
			mdb_poll_reply = MDB_REPLY_ACK;
			checksum = MDB_VEND;
			send_str_p(UPLINK_USART,PSTR("Error: invalid checksum [VEND]\r\n"));
			return;
		}
		
		sprintf(buffer, "vend-request %d %d\r\n", (data[1] + data[2]), (data[3] + data[4]));
		send_str(UPLINK_USART,buffer);
		
		// send ACK
		send_mdb(MDB_USART, 0x100);
		state = 0;
		mdb_state = MDB_VENDING;
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		return;
		break;
		
		// vend cancel
		case 1:
		// wait for enough data in buffer
		if(buffer_level(MDB_USART,RX) < 2) return;

		#if DEBUG == 1
		send_str_p(UPLINK_USART, PSTR("VEND Cancel\r\n"));
		#endif
		
		// fetch the data from buffer
		data[1] = (uint8_t) recv_mdb(MDB_USART);
		
		// calculate checksum
		checksum += data[0];
		checksum &= 0xFF;
		
		// validate checksum
		if(checksum != data[1]) {
			state = 0;
			mdb_active_cmd = MDB_IDLE;
			mdb_poll_reply = MDB_REPLY_ACK;
			checksum = MDB_VEND;
			send_str_p(UPLINK_USART,PSTR("Error: invalid checksum [VEND]\r\n"));
			return;
		}
		
		send_str_p(UPLINK_USART,PSTR("vend-cancel\r\n"));
		
		// send ACK
		send_mdb(MDB_USART, 0x100);
		state = 0;
		mdb_state = MDB_SESSION_IDLE;
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_VEND_DENIED;
		return;
		break;
		
		// vend success
		case 2:
		// wait for enough data in buffer
		if(buffer_level(MDB_USART,RX) < 6) return;

		#if DEBUG == 1
		send_str_p(UPLINK_USART, PSTR("VEND SUCCESS\r\n"));
		#endif
		
		// fetch the data from buffer
		for(uint8_t i=1; i < 4; i++) {
			data[i] = (uint8_t) recv_mdb(MDB_USART);
		}
		
		// calculate checksum
		checksum += data[0] + data[1] + data[2];
		checksum &= 0xFF;
		
		// validate checksum
		if(checksum != data[3]) {
			state = 0;
			mdb_active_cmd = MDB_IDLE;
			mdb_poll_reply = MDB_REPLY_ACK;
			checksum = MDB_VEND;
			send_str_p(UPLINK_USART,PSTR("Error: invalid checksum [VEND]\r\n"));
			return;
		}
		
		sprintf(buffer, "vend-success %d\r\n", (data[1] + data[2]));
		send_str(UPLINK_USART,buffer);
		
		// send ACK
		send_mdb(MDB_USART, 0x100);
		state = 0;
		mdb_state = MDB_SESSION_IDLE;
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		return;
		break;
		
		// vend failure
		case 3:
		// wait for enough data in buffer
		if(buffer_level(MDB_USART,RX) < 2) return;

		#if DEBUG == 1
		send_str_p(UPLINK_USART, PSTR("VEND FAILURE\r\n"));
		#endif
		
		// fetch the data from buffer
		data[1] = (uint8_t) recv_mdb(MDB_USART);
		
		// calculate checksum
		checksum += data[0];
		checksum &= 0xFF;
		
		// validate checksum
		if(checksum != data[1]) {
			state = 0;
			mdb_active_cmd = MDB_IDLE;
			mdb_poll_reply = MDB_REPLY_ACK;
			checksum = MDB_VEND;
			send_str_p(UPLINK_USART,PSTR("Error: invalid checksum [VEND]\r\n"));
			return;
		}
		
		send_str_p(UPLINK_USART,PSTR("vend-failure\r\n"));
		
		// send ACK
		send_mdb(MDB_USART, 0x100);
		state = 0;
		mdb_state = MDB_ENABLED;
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		return;
		break;
		
		// session complete
		case 4:
		// wait for enough data in buffer
		if(buffer_level(MDB_USART,RX) < 2) return;

		#if DEBUG == 1
		send_str_p(UPLINK_USART, PSTR("VEND SESSION COMPLETE\r\n"));
		#endif
		
		// fetch the data from buffer
		data[1] = (uint8_t) recv_mdb(MDB_USART);
		
		// calculate checksum
		checksum += data[0];
		checksum &= 0xFF;
		
		// validate checksum
		if(checksum != data[1]) {
			state = 0;
			mdb_active_cmd = MDB_IDLE;
			mdb_poll_reply = MDB_REPLY_ACK;
			checksum = MDB_VEND;
			send_str_p(UPLINK_USART,PSTR("Error: invalid checksum [VEND]\r\n"));
			return;
		}
		
		send_str_p(UPLINK_USART,PSTR("session-complete\r\n"));
		
		// send ACK
		send_mdb(MDB_USART, 0x100);
		state = 0;
		mdb_state = MDB_ENABLED;
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		return;
		break;
	}
}

void mdb_reader2(void) {
	data_reader_offset = 0;
	mdb_active_cmd = MDB_READER_PROCESS;
}

void mdb_reader_process(void) {
	if (buffer_level(MDB_USART,RX) < 2) return;

	data_reader_data[data_reader_offset] = (uint8_t)recv_mdb(MDB_USART);
	data_reader_offset++;
	if (data_reader_offset == data_reader_size) {
		switch(data_reader_data[0]) {
			// reader disable
			case 0:
				if(data_reader_data[1] != 0x14) {
					send_str_p(UPLINK_USART,PSTR("Error: checksum error [READER disable]\r\n"));
					mdb_active_cmd = MDB_IDLE;
					mdb_poll_reply = MDB_REPLY_ACK;
					return;
				}
			
			
				// send ACK
				send_mdb(MDB_USART, 0x100);
				mdb_active_cmd = MDB_IDLE;
				mdb_poll_reply = MDB_REPLY_ACK;
				mdb_state = MDB_DISABLED;

				#if DEBUG == 1
				send_str_p(UPLINK_USART, PSTR("READER DISABLE\r\n"));
				#endif

				break;
			
			// reader enable
			case 1:
				if(data_reader_data[1] != 0x15) {
					send_str_p(UPLINK_USART,PSTR("Error: checksum error [READER enable]\r\n"));
					mdb_active_cmd = MDB_IDLE;
					mdb_poll_reply = MDB_REPLY_ACK;
					return;
				}
			
				#if DEBUG == 1
				send_str_p(UPLINK_USART, PSTR("READER ENABLE\r\n"));
				#endif
			
				// send ACK
				send_mdb(MDB_USART, 0x100);
				mdb_active_cmd = MDB_IDLE;
				mdb_poll_reply = MDB_REPLY_ACK;
				mdb_state = MDB_ENABLED;
				break;

			// reader cancel
			case 2:
				if(data_reader_data[1] != 0x16) {
					send_str_p(UPLINK_USART,PSTR("Error: checksum error [READER cancel]\r\n"));
					mdb_active_cmd = MDB_IDLE;
					mdb_poll_reply = MDB_REPLY_ACK;
					return;
				}
			
				#if DEBUG == 1
				send_str_p(UPLINK_USART, PSTR("READER CANCEL\r\n"));
				#endif
			
				// send ACK
				send_mdb(MDB_USART, 0x108);
				mdb_active_cmd = MDB_IDLE;
				mdb_poll_reply = MDB_REPLY_CANCELED;
				mdb_state = MDB_ENABLED;
				break;

			// unknown subcommand
			default:
				send_str_p(UPLINK_USART,PSTR("Error: unknown subcommand [READER]\r\n"));
				mdb_active_cmd = MDB_IDLE;
				mdb_poll_reply = MDB_REPLY_ACK;
				return;
				break;
		}
	}
}

void mdb_expansion(void) {
	char buffer[60];
	//send_str_p(UPLINK_USART,PSTR("MDB_EXPANSION\r\n"));
	
	if (buffer_level(MDB_USART, RX) < 2) return;
	
	mdb_expansion_sub_command =  (uint8_t) recv_mdb(MDB_USART);
	//sprintf(buffer, "MDB_EXPANSION request %x\r\n", req);
	//send_str(UPLINK_USART,buffer);
	
	//send_str_p(UPLINK_USART,PSTR("ExpansionRequestID\r\n"));
	/*
	if (mdb_expansion_sub_command == 0x00) {
		//sprintf(buffer, "MDB_EXPANSION 0 request %x\r\n", req);
		//send_str(UPLINK_USART,buffer);
		data_expansion_request_id_offset = 0;
		mdb_active_cmd  = MDB_EXPANSION_REQUEST_ID;
		//ExpansionRequestID();
	}
	else {
		sprintf(buffer, "MDB_EXPANSION unknown sub command %x\r\n", mdb_expansion_sub_command);
		send_str(UPLINK_USART,buffer);
	}
	*/

	switch (mdb_expansion_sub_command) {
		case 0x00:
			data_expansion_request_id_offset = 0;
			mdb_active_cmd  = MDB_EXPANSION_REQUEST_ID;
			break;
		case 0x04:
			data_expansion_enable_options_offset = 0;
			mdb_active_cmd  = MDB_EXPANSION_ENABLE_OPTIONS;
			break;
		default:
			sprintf(buffer, "MDB_EXPANSION unknown sub command %x\r\n", mdb_expansion_sub_command);
			send_str(UPLINK_USART,buffer);
	}
}

void ExpansionRequestID(void) {
	char buffer[40];
	
	if (buffer_level(MDB_USART, RX) < 2) {
		//sprintf(buffer, "level %d\r\n", level);
		//send_str(UPLINK_USART,buffer);
		return;
	}
	
	data_expansion_request_id[data_expansion_request_id_offset] = (uint8_t)recv_mdb(MDB_USART);
	data_expansion_request_id_offset++;
	
	if (data_expansion_request_id_offset == data_expansion_request_id_size) {
		uint16_t checksum = MDB_EXPANSION + mdb_expansion_sub_command;
		checksum += calc_checksum_16(data_expansion_request_id, data_expansion_request_id_size - 1);
		checksum = checksum & 0xFF;
		uint16_t dataChecksum = data_expansion_request_id[data_expansion_request_id_size - 1];
		if (checksum != (dataChecksum & 0xFF)) {
			mdb_active_cmd = MDB_IDLE;
			mdb_expansion_sub_command = 0;
			//send_str_p(UPLINK_USART,PSTR("Error: Invalid checksum [EXPANSION REQUEST ID]\r\n"));
			
			sprintf(buffer, "checksum ExpansionRequestID %x %x\r\n", checksum, dataChecksum);
			send_str(UPLINK_USART,buffer);
			
			//for(index = 0; index < dataSize; index++) {
			//sprintf(buffer, "ExpansionRequestID %d %x\r\n", index, data[index]);
			//send_str(UPLINK_USART,buffer);
			//}
			mdb_active_cmd = MDB_IDLE;
			return;
		}

		uint8_t t = 0;
		// for(t = 0; t < data_expansion_request_id_size; t++) {
		// 	sprintf(buffer, "ExpansionRequestID %d %x\r\n", t, data_expansion_request_id[t]);
		// 	send_str(UPLINK_USART, buffer);
		// }

		for (t = 0; t < 3; t++)
			vmcDevice.manufacturerCode[t] = data_expansion_request_id[t];
		for (t = 0; t < 12; t++)
			vmcDevice.serialNumber[t] = data_expansion_request_id[3 + t];
		for (t = 0; t < 12; t++)
			vmcDevice.modelNumber[t] = data_expansion_request_id[15 + t];
		vmcDevice.softwareVersion = ((data_expansion_request_id[27] & 0xFF) << 8) | (data_expansion_request_id[28] & 0xff);
		
		SendPeripheralID();
		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_PERIPHERIAL_ID;
		send_str_p(UPLINK_USART, PSTR("MDB_EXPANSION ExpansionRequestID FINISH\r\n"));
	}
	
	//sprintf(buffer, "level  complete %d\r\n", level);
	//send_str(UPLINK_USART,buffer);
	
	//uint8_t dataSize = 40;
	//uint16_t data[dataSize];
	//uint8_t index;
	//uint16_t checksum = MDB_EXPANSION + mdb_expansion_sub_command;
	//
	//for(index = 0; index < dataSize; index++) {
		//data[index] = recv_mdb(MDB_USART);
		////sprintf(buffer, "ExpansionRequestID %d %x\r\n", index, data[index]);
		////send_str(UPLINK_USART,buffer);
	//}
	//checksum += calc_checksum_16(data, dataSize - 1);
	//checksum = checksum & 0xFF;
	//
	//uint16_t dataChecksum = data[dataSize - 1];
	//
	//// validate checksum
	//if (checksum != (dataChecksum & 0xFF)) {
		//mdb_active_cmd = MDB_IDLE;
		//mdb_expansion_sub_command = 0;
		////send_str_p(UPLINK_USART,PSTR("Error: Invalid checksum [EXPANSION REQUEST ID]\r\n"));
		//
		//sprintf(buffer, "checksum %x %x\r\n", checksum, dataChecksum);
		//send_str(UPLINK_USART,buffer);
		//
		////for(index = 0; index < dataSize; index++) {
			////sprintf(buffer, "ExpansionRequestID %d %x\r\n", index, data[index]);
			////send_str(UPLINK_USART,buffer);
		////}
		//mdb_active_cmd = MDB_TEST;
		//return;
	//}
	//
	////sprintf(buffer, "checksum %x, data[14]: %x\r\n", checksum, data[dataSize - 1]);
	////send_str(UPLINK_USART,buffer);
	//
	//send_str_p(UPLINK_USART,PSTR("MDB_EXPANSION FINISH\r\n"));
	//SendPeripheralID();
	//mdb_active_cmd = MDB_TEST;
}

void ExpansionEnableOptions(void) {
	char buffer[40];
	
	if (buffer_level(MDB_USART, RX) < 2) {
		return;
	}
	
	data_expansion_enable_options[data_expansion_enable_options_offset] = (uint8_t)recv_mdb(MDB_USART);
	data_expansion_enable_options_offset = data_expansion_enable_options_offset + 1;
	
	if (data_expansion_enable_options_offset == data_expansion_enable_options_size) {
		uint16_t checksum = MDB_EXPANSION + mdb_expansion_sub_command;
		checksum += calc_checksum_16(data_expansion_enable_options, data_expansion_enable_options_size - 1);
		checksum = checksum & 0xFF;
		uint16_t dataChecksum = data_expansion_enable_options[data_expansion_enable_options_size - 1];
		if (checksum != (dataChecksum & 0xFF)) {
			mdb_active_cmd = MDB_IDLE;
			mdb_expansion_sub_command = 0;
			sprintf(buffer, "checksum ExpansionEnableOptions %x %x\r\n", checksum, dataChecksum);
			send_str(UPLINK_USART,buffer);
			
			//for(index = 0; index < dataSize; index++) {
			//sprintf(buffer, "ExpansionRequestID %d %x\r\n", index, data[index]);
			//send_str(UPLINK_USART,buffer);
			//}
			mdb_active_cmd = MDB_IDLE;
			return;
		}

		vmcExpansionEnableOptions.cfg1 = data_expansion_enable_options[0];
		vmcExpansionEnableOptions.cfg2 = data_expansion_enable_options[1];
		vmcExpansionEnableOptions.cfg3 = data_expansion_enable_options[2];
		vmcExpansionEnableOptions.cfg4 = data_expansion_enable_options[3];

		mdb_active_cmd = MDB_IDLE;
		mdb_poll_reply = MDB_REPLY_ACK;
		
		send_mdb(MDB_USART, 0x100);

		send_str_p(UPLINK_USART, PSTR("MDB_EXPANSION ExpansionEnableOptions FINISH\r\n"));
	}
}

#define periph_id_size 34

void SendPeripheralID() {
	// uint8_t periph_id[30];
	uint8_t periph_id[periph_id_size];
	uint8_t i;
	// uint8_t a_char = 97;
	periph_id[0] = 0x09;

	// Manufacturer Code - ASCII
	periph_id[1] = 'F';
	periph_id[2] = 'T';
	periph_id[3] = 'P';
	
	// Serial Number – ASCII 000000000001 ASCII
	for (i = 4; i < 15; ++i) {
		periph_id[i] = '0';
	}
	periph_id[15] = '1';

	// Model Number - ASCII 
	for (i = 16; i < 28; ++i) {
		periph_id[i] = '0';
	}
	// Software Version - packed BCD
	periph_id[28] = 1;
	periph_id[29] = 0;
	// ---------------------------------
	periph_id[30] = 0; // 0b01100110;
	periph_id[31] = 0;
	periph_id[32] = 0;
	periph_id[33] = 0xFF;

	uint8_t checksum = calc_checksum(periph_id, periph_id_size);
	// Send all data on the bus
	for (i = 0; i < periph_id_size; ++i)
		send_mdb(MDB_USART, periph_id[i]);
		
	send_mdb(MDB_USART, checksum | 0x100);
}

uint8_t calc_checksum(uint8_t *array, uint8_t arr_size)
{
	uint8_t ret_val = 0x00;
	uint8_t i;
	for (i = 0; i < arr_size; ++i)
	ret_val += *(array + i);
	return ret_val;
}

uint16_t calc_checksum_16(uint16_t *array, uint8_t arr_size)
{
	uint16_t ret_val = 0x00;
	uint8_t i;
	for (i = 0; i < arr_size; ++i) {
		ret_val += *(array + i);
	}
	return ret_val;
}