#pragma once
#include <stdint.h>
#define SCE_TOUCH_MAX_REPORT 8
enum { SCE_TOUCH_PORT_FRONT = 0, SCE_TOUCH_PORT_BACK = 1 };
enum { SCE_TOUCH_SAMPLING_STATE_START = 1 };
/* Field types and offsets mirror the real VitaSDK touch.h used by the port. */
typedef struct {
    uint8_t id, force;
    int16_t x, y;
    uint8_t reserved[8];
    uint16_t info;
} SceTouchReport;
typedef struct {
    uint64_t timeStamp;
    uint32_t status, reportNum;
    SceTouchReport report[SCE_TOUCH_MAX_REPORT];
} SceTouchData;
int sceTouchPeek(uint32_t port, SceTouchData *data, uint32_t buffers);
int sceTouchSetSamplingState(uint32_t port, uint32_t state);
