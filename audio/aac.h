#ifndef _AAC_H_
#define _AAC_H_

void *aac_new_decoder(void);
void free_aac_decoder(void *aac);
void aac_set_config(void *aac, int sample_rate, int mono_stereo);
void *decode_au(void *_a, unsigned char *data, int len, int *outlen);

#endif /* _AAC_H_ */
