/*
 * Cross-language shared configuration file.
 * Copy this SAME text into a C header (.h) and a Python module (.py).
 * C will use the #define constants; Python will execute the loader in the #if 0 block
 * to convert the #define lines into Python variables.
 *
 * Usage (C):
 *   #include "SHARED_CONFIG.h"
 *   // constants: INFO_VBUS, SENSOR_FAN, RAW_DAC1, DEVICE_NAME, TLV_DEBUG_ENABLE ...
 *
 * Usage (Python):
 *   import SHARED_CONFIG  # after copying content to SHARED_CONFIG.py
 *   print(SHARED_CONFIG.INFO_VBUS)
 *
 * Notes:
 * - Avoid putting non-#define C code outside the #if 0 block so Python can parse.
 * - Add new constants by appending more "#define NAME value" lines; both languages pick them up.
 */

/* Debug control */
#define TLV_DEBUG_ENABLE 0  /* 1: enable protocol debug prints; 0: disable */

/* Info IDs */
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

/* Sensor IDs */
#define SENSOR_FAN  0x21
#define SENSOR_TPS  0x22
#define SENSOR_INA  0x23
#define SENSOR_TEMP 0x24

/* Raw IDs */
#define RAW_DAC1    0x31
#define RAW_DAC2    0x32
#define RAW_ADC     0x33
#define RAW_PID1    0x34
#define RAW_PID2    0x35

/* Board / Device IDs */
#define DEVICE_NAME   0xDA
#define DEVICE_UID    0xDB
#define DEVICE_REV    0xDC
#define DEVICE_FLASH  0xDE
#define DEVICE_ESPID  0xDF
#define DEVICE_ESPFU  0xFF

/* Additional TLV */
#define TLV_TYPE_CONTROL_CMD 0x10
#define TLV_TYPE_INTEGER     0x20
#define TLV_TYPE_STRING      0x30
#define TLV_TYPE_ACK         0x06
#define TLV_TYPE_NACK        0x15

/* Python loader block: ignored by C because inside #if 0 ... #endif */
#if 0
# --- Python constant auto-loader ---
# Scans this same file for lines: #define NAME VALUE and creates Python globals.
import re, os
with open(__file__, 'r', encoding='utf-8') as _f:
    for _line in _f:
        _m = re.match(r'^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)', _line)
        if _m:
            globals()[_m.group(1)] = int(_m.group(2), 0)
# cleanup temp names
for _n in ['_f', '_line', '_m', 're', 'os', '_n']:
    if _n in globals():
        try: del globals()[_n]
        except: pass
#endif

