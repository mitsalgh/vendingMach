/**
 * Project: MateDealer
 * File name: mdb.h
 * Description:  MultiDropBus methods. 
 *               See www.vending.org/technology/MDB_Version_4-2.pdf for Protocoll information.
 *   
 * @author bouni
 * @email bouni@owee.de  
 *   
 */
 
#ifndef MDB_H
#define MDB_H

#ifndef F_CPU
#define F_CPU       16000000UL
#endif

#ifndef TRUE
#define TRUE        1
#endif

#ifndef FALSE
#define FALSE       0
#endif

//#define DEBUG       0

#define MDB_USART   1

//---------------------------------------------------------------------------
//  MDB STATES
//---------------------------------------------------------------------------
enum MDB_STATES {MDB_INACTIVE,MDB_DISABLED,MDB_ENABLED,MDB_SESSION_IDLE,MDB_VENDING,MDB_REVALUE,MDB_NEGATIVE_VEND};

//---------------------------------------------------------------------------
//  MDB CMDS
//---------------------------------------------------------------------------
enum MDB_CMD {
	MDB_IDLE,
	MDB_RESET = 0x10,
	MDB_SETUP, // 0x11
	MDB_POLL, // 0x12
	MDB_VEND, // 0x13
	MDB_READER, // 0x14
	MDB_REVALUE_ADDR, // 0x15
	MDB_EXPANSION = 0x17, // 0x17
    // custom process
	MDB_EXPANSION_REQUEST_ID = 0x21,
    MDB_EXPANSION_ENABLE_OPTIONS = 0x22,
    MDB_READER_PROCESS = 0x23,
	MDB_TEST = 0x99
};

//---------------------------------------------------------------------------
//  POLL REPLYS
//---------------------------------------------------------------------------
enum POLL_REPLY {
	MDB_REPLY_ACK,
	MDB_REPLY_JUST_RESET,
	MDB_REPLY_READER_CFG,
	MDB_REPLY_DISPLAY_REQ,
	MDB_REPLY_BEGIN_SESSION,
    MDB_REPLY_SESSION_CANCEL_REQ,
	MDB_REPLY_VEND_APPROVED,
	MDB_REPLY_VEND_DENIED,
	MDB_REPLY_END_SESSION,
    MDB_REPLY_CANCELED,
	MDB_REPLY_PERIPHERIAL_ID,
	MDB_REPLY_ERROR,
	MDB_REPLY_CMD_OUT_SEQUENCE,
    MDB_REPLY_IDLE
};

enum MDB_SETUP_STATE {
    MDB_SETUP_STATE_IDLE,
    MDB_SETUP_STATE_CONFIG_DATA,
    MDB_SETUP_STATE_MIN_MAX
};

typedef struct {
    uint8_t manufacturerCode[3];
    uint8_t serialNumber[12];
    uint8_t modelNumber[12];
    uint16_t softwareVersion;
} vmcDevice_t;

typedef struct {
    uint8_t cfg1;
    uint8_t cfg2;
    uint8_t cfg3;
    uint8_t cfg4;
} vmcExpansionEnableOptions_t;
                
typedef struct {
    uint8_t feature_level;
    uint8_t dispaly_cols;
    uint8_t dispaly_rows;
    uint8_t dispaly_info;
} vmcCfg_t;

typedef struct {
    uint8_t  reader_cfg;
    uint8_t  feature_level;
    uint16_t country_code;
    uint8_t  scale_factor;
    uint8_t  decimal_places;
    uint8_t  max_resp_time;
    uint8_t  misc_options;
} cdCfg_t;

typedef struct {
    uint16_t max;
    uint16_t min;
    uint16_t maxLow;
    uint16_t minLow;
    uint16_t currency;
} vmcPrice_t;


typedef struct {
    uint8_t flag;
    uint16_t funds;
} start_t;

typedef struct {
    uint8_t vend_approved;
    uint8_t vend_denied;
    uint16_t vend_amount;
} result_t;


typedef struct {
    start_t start;
    result_t result;
} mdbSession_t;

uint8_t mdb_state;

#define data_reader_size 2
uint8_t data_reader_offset;
uint16_t data_reader_data[data_reader_size];

#define data_expansion_request_id_size 30
uint8_t data_expansion_request_id_offset;
uint16_t data_expansion_request_id[data_expansion_request_id_size]; // include checksum

#define data_expansion_enable_options_size 5
uint8_t data_expansion_enable_options_offset;
uint16_t data_expansion_enable_options[data_expansion_enable_options_size]; // include checksum

void forward_to_pc(void);
void forward_to_mdb(void);

void mdb_cmd_handler(void);
void mdb_reset(void);
void mdb_setup(void);
void mdb_setup_config_data(void);
void mdb_setup_min_max_level_1_2(void);
void mdb_setup_min_max_level_3(void);
void mdb_poll(void);
void mdb_poll_2(void);
void mdb_vend(void);
void mdb_reader2(void);
void mdb_reader_process(void);

void mdb_expansion(void);
void ExpansionRequestID(void);
void ExpansionEnableOptions(void);
void SendPeripheralID();
void MdbTest(void);

void mdb_send_config_reader(void);

uint8_t calc_checksum(uint8_t *array, uint8_t arr_size);
uint16_t calc_checksum_16(uint16_t *array, uint8_t arr_size);

#endif // MDB_H
