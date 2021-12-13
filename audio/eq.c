/* ripped from the infamous videolan (you suck and I'm lazy today) */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define EQZ_IN_FACTOR (0.25)
#define EQZ_BANDS_MAX 10

typedef struct
{
    const char *psz_name;
    int  i_band;
    float f_preamp;
    float f_amp[EQZ_BANDS_MAX];
} eqz_preset_t;

static const eqz_preset_t eqz_preset_fullbass_10b=
{
    "fullbass", 10, 0.0,
    { 9.6, 9.6, 9.6, 5.6, 1.6, -4, -8, -10.4, -11.2, -11.2  }
};

typedef struct
{
    int   i_band;

    struct
    {
        float f_frequency;
        float f_alpha;
        float f_beta;
        float f_gamma;
    } band[EQZ_BANDS_MAX];

} eqz_config_t;

static const eqz_config_t eqz_config_48000_10b =
{
    10,
    {
        {    60, 0.003013, 0.993973, 1.993901 },
        {   170, 0.008490, 0.983019, 1.982437 },
        {   310, 0.015374, 0.969252, 1.967331 },
        {   600, 0.029328, 0.941343, 1.934254 },
        {  1000, 0.047918, 0.904163, 1.884869 },
        {  3000, 0.130408, 0.739184, 1.582718 },
        {  6000, 0.226555, 0.546889, 1.015267 },
        { 12000, 0.344937, 0.310127, -0.181410 },
        { 14000, 0.366438, 0.267123, -0.521151 },
        { 16000, 0.379009, 0.241981, -0.808451 },
    }
};

struct filter_sys_t
{
    /* Filter static config */
    int i_band;
    float *f_alpha;
    float *f_beta;
    float *f_gamma;

    float f_newpreamp;
    char *psz_newbands;
    int b_first;

    /* Filter dyn config */
    float *f_amp;   /* Per band amp */
    float f_gamp;   /* Global preamp */
    int b_2eqz;

    /* Filter state */
    float x[32][2];
    float y[32][128][2];

    /* Second filter state */
    float x2[32][2];
    float y2[32][128][2];
};

static float EqzFilter(struct filter_sys_t *p_sys, float in)
{
    int ch = 0, j;

            const float x = in;
            float o = 0.0;

            for( j = 0; j < p_sys->i_band; j++ )
            {
                float y = p_sys->f_alpha[j] * ( x - p_sys->x[ch][1] ) +
                          p_sys->f_gamma[j] * p_sys->y[ch][j][0] -
                          p_sys->f_beta[j]  * p_sys->y[ch][j][1];

                p_sys->y[ch][j][1] = p_sys->y[ch][j][0];
                p_sys->y[ch][j][0] = y;

                o += y * p_sys->f_amp[j];
            }
            p_sys->x[ch][1] = p_sys->x[ch][0];
            p_sys->x[ch][0] = x;

                /* We add source PCM + filtered PCM */
                return p_sys->f_gamp *( EQZ_IN_FACTOR * x + o );
}

struct filter_sys_t p_sys_left;
struct filter_sys_t p_sys_right;

void filter(short ina, short inb, short *outa, short *outb)
{
  float l = EqzFilter(&p_sys_left, ina);
  float r = EqzFilter(&p_sys_right, inb);
  if (l < -32768) l = -32768;
  if (l > 32767) l = 32767;
  if (r < -32768) r = -32768;
  if (r > 32767) r = 32767;
  *outa = l;
  *outb = r;
}

static inline float EqzConvertdB( float db )
{
    /* Map it to gain,
     * (we do as if the input of iir is /EQZ_IN_FACTOR, but in fact it's the non iir data that is *EQZ_IN_FACTOR)
     * db = 20*log( out / in ) with out = in + amp*iir(i/EQZ_IN_FACTOR)
     * or iir(i) == i for the center freq so
     * db = 20*log( 1 + amp/EQZ_IN_FACTOR )
     * -> amp = EQZ_IN_FACTOR*(10^(db/20) - 1)
     **/

    if( db < -20.0 )
        db = -20.0;
    else if(  db > 20.0 )
        db = 20.0;
    return EQZ_IN_FACTOR * ( pow( 10, db / 20.0 ) - 1.0 );
}

static void EqzInit(struct filter_sys_t *p_sys, int i_rate )
{
    const eqz_config_t *p_cfg;
    int i, ch, j;

    /* Select the config */
        p_cfg = &eqz_config_48000_10b;

    /* Create the static filter config */
    p_sys->i_band = p_cfg->i_band;
    p_sys->f_alpha = malloc( p_sys->i_band * sizeof(float) );
    p_sys->f_beta  = malloc( p_sys->i_band * sizeof(float) );
    p_sys->f_gamma = malloc( p_sys->i_band * sizeof(float) );
    if( !p_sys->f_alpha || !p_sys->f_beta || !p_sys->f_gamma )
        goto error;

    for( i = 0; i < p_sys->i_band; i++ )
    {
        p_sys->f_alpha[i] = p_cfg->band[i].f_alpha;
        p_sys->f_beta[i]  = p_cfg->band[i].f_beta;
        p_sys->f_gamma[i] = p_cfg->band[i].f_gamma;
    }

    /* Filter dyn config */
    p_sys->b_2eqz = 0;
    p_sys->f_gamp = 1.0;
    p_sys->f_amp  = malloc( p_sys->i_band * sizeof(float) );
    if( !p_sys->f_amp )
        goto error;

    for( i = 0; i < p_sys->i_band; i++ )
    {
        p_sys->f_amp[i] = 0.0;
    }

    /* Filter state */
    for( ch = 0; ch < 32; ch++ )
    {
        p_sys->x[ch][0]  =
        p_sys->x[ch][1]  =
        p_sys->x2[ch][0] =
        p_sys->x2[ch][1] = 0.0;

        for( i = 0; i < p_sys->i_band; i++ )
        {
            p_sys->y[ch][i][0]  =
            p_sys->y[ch][i][1]  =
            p_sys->y2[ch][i][0] =
            p_sys->y2[ch][i][1] = 0.0;
        }
    }

#if 0
    p_sys->b_first = true;
    PresetCallback( VLC_OBJECT( p_aout ), NULL, val1, val1, p_sys );
    BandsCallback(  VLC_OBJECT( p_aout ), NULL, val2, val2, p_sys );
    PreampCallback( VLC_OBJECT( p_aout ), NULL, val3, val3, p_sys );
    p_sys->b_first = false;
#endif

    /* preset */
            char *psz_newbands = NULL;

            p_sys->f_gamp *= pow( 10, eqz_preset_fullbass_10b.f_preamp / 20.0 );
            for(j = 0; j < p_sys->i_band; j++ )
            {
                lldiv_t d;
                char *psz;

                p_sys->f_amp[j] = EqzConvertdB( eqz_preset_fullbass_10b.f_amp[j] );
                d = lldiv( eqz_preset_fullbass_10b.f_amp[j] * 10000000, 10000000 );
                if( asprintf( &psz, "%s %lld.%07llu",
                              psz_newbands ? psz_newbands : "",
                              d.quot, d.rem ) == -1 )
                {
                    free( psz_newbands ); exit(1);
                }
                free( psz_newbands );
                psz_newbands = psz;
            }
                p_sys->psz_newbands = psz_newbands;
                p_sys->f_newpreamp = eqz_preset_fullbass_10b.f_preamp;

    /* bands */
    for(i = 0; i < p_sys->i_band; i++ )
    {
        float f;

        /* Read dB -20/20 */
        f = eqz_preset_fullbass_10b.f_amp[i];

        p_sys->f_amp[i] = EqzConvertdB( f );

    }

    /* preamp */
//    p_sys->f_gamp = 1.;

    /* Register preset bands (for intf) if : */
    /* We have no bands info --> the preset info must be given to the intf */
    /* or The bands info matches the preset */
    if (p_sys->psz_newbands == NULL)
    {
        free( p_sys->f_amp );
        goto error;
    }

    return;

error:
  exit(1);
}

void init_filter(void)
{
  EqzInit(&p_sys_left, 48000);
  EqzInit(&p_sys_right, 48000);
}
