#pragma once

#define AML_ZERO_OP         0x00
#define AML_ONE_OP          0x01
#define AML_ALIAS_OP        0x06
#define AML_NAME_OP         0x08
#define AML_BYTE_PREFIX     0x0A
#define AML_WORD_PREFIX     0x0B
#define AML_DWORD_PREFIX    0x0C
#define AML_STRING_PREFIX   0x0D
#define AML_QWORD_PREFIX    0x0E
#define AML_SCOPE_OP        0x10
#define AML_BUFFER_OP       0x11
#define AML_PACKAGE_OP      0x12
#define AML_VAR_PACKAGE_OP  0x13
#define AML_METHOD_OP       0x14
#define AML_DUAL_NAME_PFX   0x2E
#define AML_MULTI_NAME_PFX  0x2F
#define AML_EXT_OP_PREFIX   0x5B
#define AML_ROOT_CHAR       0x5C
#define AML_PARENT_CHAR     0x5E
#define AML_ONES_OP         0xFF

#define AML_EXT_MUTEX_OP        0x01
#define AML_EXT_OPREGION_OP     0x80
#define AML_EXT_FIELD_OP        0x81
#define AML_EXT_DEVICE_OP       0x82
#define AML_EXT_PROCESSOR_OP    0x83
#define AML_EXT_POWERRES_OP     0x84
#define AML_EXT_THERMALZONE_OP  0x85
