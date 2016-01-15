#ifndef _CRC_H
#define _CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t checksum(unsigned char *data, int bytes);

#ifdef __cplusplus
}
#endif

#endif
