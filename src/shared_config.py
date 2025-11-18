"""
Shared configuration (C & Python compatible).
Copy of SHARED_CONFIG.h content. Python will parse #define lines.
"""
# Debug control
# define lines kept verbatim for easier sync
#define TLV_DEBUG_ENABLE 0  /* 1: enable protocol debug prints; 0: disable */

#define INFO_VBUS   0x01
#define INFO_IBUS   0x03
#define INFO_PBUS   0x05
#define INFO_BUS    0x07
#define INFO_VOUT   0x11
#define INFO_IOUT   0x13
#define INFO_POUT   0x15
#define INFO_OUT    0x17
#define INFO_VSET   0x09
#define INFO_ISET   0x19

#define SENSOR_FAN  0x21
#define SENSOR_TPS  0x22
#define SENSOR_INA  0x23
#define SENSOR_TEMP 0x24

#define RAW_DAC1    0x31
#define RAW_DAC2    0x32
#define RAW_ADC     0x33
#define RAW_PID1    0x34
#define RAW_PID2    0x35

#define DEVICE_NAME   0xDA
#define DEVICE_UID    0xDB
#define DEVICE_REV    0xDC
#define DEVICE_FLASH  0xDE
#define DEVICE_ESPID  0xDF
#define DEVICE_ESPFU  0xFF

#define TLV_TYPE_CONTROL_CMD 0x10
#define TLV_TYPE_INTEGER     0x20
#define TLV_TYPE_STRING      0x30
#define TLV_TYPE_ACK         0x06
#define TLV_TYPE_NACK        0x15

# Auto loader: convert #define lines to Python variables
import re as _re
import os as _os
with open(__file__, 'r', encoding='utf-8') as _f:
    for _line in _f:
        m = _re.match(r'^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)', _line)
        if m:
            globals()[m.group(1)] = int(m.group(2), 0)
for _sym in ['_re','_os','_f','_line','m','_sym']:
    if _sym in globals():
        try: del globals()[_sym]
        except: pass

