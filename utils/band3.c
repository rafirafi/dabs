#include "band3.h"

band_t band3[] = {
{(char*)"5A", 174.928},
{(char*)"5B", 176.640},
{(char*)"5C", 178.352},
{(char*)"5D", 180.064},
{(char*)"6A", 181.936},
{(char*)"6B", 183.648},
{(char*)"6C", 185.360},
{(char*)"6D", 187.072},
{(char*)"7A", 188.928},
{(char*)"7B", 190.640},
{(char*)"7C", 192.352},
{(char*)"7D", 194.064},
{(char*)"8A", 195.936},     /* busy out3 */  /* busy out1 */ /* busy out2 (too loud) */
{(char*)"8B", 197.648},     /* busy out2 */
{(char*)"8C", 199.360},
{(char*)"8D", 201.072},     /* busy out3 */ /* busy out2 (loud) */
{(char*)"9A", 202.928},
{(char*)"9B", 204.640},
{(char*)"9C", 206.352},
{(char*)"9D", 208.064},     /* busy? out2 */
{(char*)"10A", 209.936},
{(char*)"10B", 211.648},
{(char*)"10C", 213.360},
{(char*)"10D", 215.072},
{(char*)"10N", 210.096},
{(char*)"11A", 216.928},    /* busy out3 */ /* busy out1 */ /* busy out2 (loud) */
{(char*)"11B", 218.640},    /* busy out2 */
{(char*)"11C", 220.352},    /* busy out3 */ /* busy out1 */ /* busy out2 */
{(char*)"11D", 222.064},
{(char*)"11N", 217.088},    /* busy out3 */ /* busy out1 */ /* busy out2 */
{(char*)"12A", 223.936},
{(char*)"12B", 225.648},    /* busy out3 */ /* busy out1 */ /* busy out2 */
{(char*)"12C", 227.360},    /* busy out3 (low) */ /* busy out2 */
{(char*)"12D", 229.072},
{(char*)"12N", 224.096},    /* busy out2 */
{(char*)"13A", 230.784},
{(char*)"13B", 232.496},
{(char*)"13C", 234.208},
{(char*)"13D", 235.776},
{(char*)"13E", 237.488},
{(char*)"13F", 239.200}
};

int band3_size = sizeof(band3) / sizeof(band3[0]);
