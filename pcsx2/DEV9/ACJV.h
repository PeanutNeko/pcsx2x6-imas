
// INVESTIGATE: this addres is set to 0 in 3 situations: JVFIRM upload begins, ACCORE starts, `acJvModuleStop()` is called
#define ACJV_UNK_RESET_NOTICE 0x12416000

// base address where `mc0:ACJVLD` copies `mc0:JVFIRM` into
#define JVFIRM_DESTINATION_ADDR 0x12400000
#define JVFIRM_RANGE 0x1240

//https://github.com/TheOnlyJoey/openjvs/wiki/Command-list
enum JVS_CMDS {
    /// broadcast commands (meant for all active nodes, sent to node ID FF)
    RESET = 0xF0, // [REQUIRED]
    SET_NODE_ADDRESS = 0xF1, // [REQUIRED]
    SETUP_COMMS = 0xF2,
    /// initialization commands
    READ_ID_DATA = 0x10, // [REQUIRED]
    GET_CMDFORMAT_REV = 0x11, // [REQUIRED] get command format revision
    GET_REVISION = 0x12, // [REQUIRED] Get JVS revision
    GET_SUPP_COMM_VER= 0x13, // [REQUIRED] get supported communications versions
    GET_SLAVE_FEAT = 0x14, // [REQUIRED] indicates which commands of this enum are supported
    CONVEY_ID_MAINBOARD = 0x15, // convey ID of main board
    /// I/O commands
    READ_INP_SWITCH = 0x20, //read switch inputs
    READ_INP_COIN = 0x21, //read coin inputs
    READ_INP_ANALOG = 0x22, //read analog inputs
    READ_INP_ROTATORY = 0x23, //read rotary inputs
    READ_INP_KEYCODE = 0x24, //read key code input?
    READ_INP_SCREENPOS = 0x25, //read screen position input (duckhunt gun style)
    READ_INP_GENERAL_PURPOSE = 0x26, //read general-purpose (unrelated to player) input
    UNKNOWN_2e = 0x2e, //something with payouts?
    RETRANSIT_DATA_ON_FAIL = 0x2f, // [REQUIRED] request to retransmit data in case of checksum failure
    DECREASE_COIN_NUM = 0x30, //decrease number of coins
    OUTPUT_NUM_PAYOUT = 0x31, //output the number of payouts?
    OUTPUT_GENERAL = 0x32, //general-purpose output
    PUTPUT_ANALOG = 0x33, //analog output
    OUTPUT_CHAR_DATA = 0x34, //output character data, e.g. to LCD
    OUTPUT_COIN_NUM = 0x35, //output the total number of coins?
    SUBSTRACT_PAYOUT = 0x36, //subtract payouts?
    OUTPUT_GENERAL_PURPOSE2 = 0x37, //general-purpose output 2
    OUTPUT_GENERAL_PURPOSE3 = 0x38, //general-purpose output 3
    /// Manufacturer-specific
    //60-7F	 	reserved for manufacturer-specific commands

};