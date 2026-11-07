#pragma once

#define SBI_EID_HSM       0x48534d
#define SBI_EID_SET_TIMER 0x54494D45

#define SBI_HSM_HART_START      0
#define SBI_HSM_HART_GET_STATUS 2

#define SBI_HSM_STATE_STARTED   0
#define SBI_HSM_STATE_STOPPED   1

typedef struct {
    long error;
    long value;
} sbicall_result_t;

extern sbicall_result_t sbicall(int eid, int fid, ...);
