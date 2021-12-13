#ifndef _CRC_H_
#define _CRC_H_

#define CRC_NO_COMPLEMENT 0
#define CRC_COMPLEMENT    1

int crc(char *data, int len, int poly, int crcsize, int init_value,
        int do_complement);

#endif /* _CRC_H_ */
