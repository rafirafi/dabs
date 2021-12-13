#include "fib.h"

#include <stdio.h>
#include <string.h>

#include "utils/log.h"

/* return 1 if error, 0 if ok */
int receive_fib(frame_figs *figs, unsigned char *fib, int fib_id)
{
  unsigned char *cur;
  int id;
  int len;
  int processed = 0;
  int i;
  int figs_size = figs->size;
  fig *f;

debug("processing fib %d [", fib_id);
for (i = 0; i < 30; i++) debug(" %2.2x", fib[i]);
debug("]\n");

  cur = fib;

  while (processed != 30) {
    if (cur[0] == 0xff)
      break;

    f = &figs->f[figs->size];
    figs->size++;

    memset(f, 0, sizeof(fig));

    id = cur[0] >> 5;
    len = cur[0] & 0x1f;
    cur++;
    processed++;

    f->fib_id = fib_id;

    if (processed + len > 30) goto error;

    debug("fig %d len %d id %d\n", id, len, fib_id);

    switch (id) {
    case 0: {
      int l = len - 1;
      unsigned char *sc = &cur[1];
      int cn        = cur[0] >> 7;
      int oe        = (cur[0] >> 6) & 1;
      int pd        = (cur[0] >> 5) & 1;
      int extension = cur[0] & 0x1f;
      debug("    fig 0 cn        %d\n", cn);
      debug("    fig 0 oe        %d\n", oe);
      debug("    fig 0 pd        %d\n", pd);
      debug("    fig 0 extension %d\n", extension);

      f->type = FIG0;
      f->u.fig0.cn = cn;
      f->u.fig0.oe = oe;
      f->u.fig0.pd = pd;
      f->u.fig0.extension = extension;

      switch (extension) {
      case 0: {
        if (l < 4) {
          debug("bad fig 0/0\n");
          goto error;
        }

        f->type = FIG0_0;

        int country_id = sc[0] >> 4;
        int ensemble_ref = ((sc[0] & 0x0f) << 4) | sc[1];
        int change_flags = sc[2] >> 6;
        int al_flag = (sc[2] >> 5) & 1;
        int cif_count = (sc[2] & 0x1f) * 250 + sc[3];
        int occurence_change = 0;

        f->u.fig0_0.country_id = country_id;
        f->u.fig0_0.ensemble_reference = ensemble_ref;
        f->u.fig0_0.change_flags = change_flags;
        f->u.fig0_0.al_flag = al_flag;
        f->u.fig0_0.cif_count = cif_count;

        if (change_flags && l < 5) {
          debug("bad fig 0/0\n");
          goto error;
        }

        if (change_flags) {
          occurence_change = sc[4];
          f->u.fig0_0.occurence_change = occurence_change;
        }

        debug("        country id %d\n", country_id);
        debug("        ensemble reference %d\n", ensemble_ref);
        debug("        change flags %d\n", change_flags);
        debug("        al flag %d\n", al_flag);
        debug("        cif count %d (%d %d)\n", cif_count, sc[2] & 0x1f, sc[3]);
        if (change_flags)
          debug("        occurence change %d\n", occurence_change);

        break;
      }
      case 1: {
        fig0_1 *ff = &f->u.fig0_1;

        f->type = FIG0_1;
        ff->n = 0;

        while (l > 0) {
          int sc_id;
          int start_address;
          int is_long;

          if (l < 3 || ((sc[2] & 0x80) && l < 4))
            goto error;

          ff->n++;

          sc_id = sc[0] >> 2;
          start_address = ((sc[0] & 0x3) << 8) | sc[1];
          is_long = sc[2] >> 7;

          ff->sub_channel[ff->n-1].sc_id = sc_id;
          ff->sub_channel[ff->n-1].start_address = start_address;
          ff->sub_channel[ff->n-1].is_long = is_long;

          debug("        --------------------------\n");
          debug("        subchannel id %d\n", sc_id);
          debug("        start address %d\n", start_address);
          debug("        is_long %d\n", !!is_long);

          if (!is_long) {
            int table_switch = (sc[2] >> 6) & 1;
            int table_index = sc[2] & 0x3f;

            ff->sub_channel[ff->n-1].table_switch = table_switch;
            ff->sub_channel[ff->n-1].table_index = table_index;

            debug("        table_switch %d\n", table_switch);
            debug("        table_index %d\n", table_index);
          } else {
            int option = (sc[2] >> 4) & 0x3;
            int protection_level = (sc[2] >> 2) & 0x3;
            int sc_size = ((sc[2] & 0x3) << 8) | sc[3];

            ff->sub_channel[ff->n-1].option = option;
            ff->sub_channel[ff->n-1].protection_level = protection_level;
            ff->sub_channel[ff->n-1].sc_size = sc_size;

            debug("        option %d\n", option);
            debug("        protection level %d\n", protection_level);
            debug("        subchannel size %d\n", sc_size);
          }

          if (is_long) {
            l -= 4;
            sc += 4;
          } else {
            l -= 3;
            sc += 3;
          }
        }

        break;
      }
      case 2: {
        fig0_2 *ff = &f->u.fig0_2;

        f->type = FIG0_2;
        ff->n = 0;

        while (l > 0) {
          int ecc;
          int country_id;
          int service_ref;
          int ca_id;
          int n;
          int tm_id;
          int asct;
          int dsct;
          int scid;
          int ps;
          int ca_flag;
          int k;

          if (l < 3 || (pd && l < 4))
            goto error;

          ff->n++;

          if (pd) {
            ecc = sc[0];
            country_id = sc[1] >> 4;
            service_ref = ((sc[1] & 0xf) << 16) | (sc[2] << 8) | sc[3];
            sc += 4;
            l -= 4;

            ff->service[ff->n-1].ecc = ecc;
          } else {
            country_id = sc[0] >> 4;
            service_ref = ((sc[0] & 0xf) << 8) | sc[1];
            sc += 2;
            l -= 2;
          }

          ff->service[ff->n-1].country_id = country_id;
          ff->service[ff->n-1].service_reference = service_ref;

          debug("        --------------------------\n");
          if (pd) debug("        ecc %d\n", ecc);
          debug("        country id %d\n", country_id);
          debug("        service_ref %d\n", service_ref);

          ca_id = (sc[0] >> 4) & 0x7;

          ff->service[ff->n-1].ca_id = ca_id;

          n = sc[0] & 0xf;

          ff->service[ff->n-1].n = n;

          debug("        ca_id %d\n", ca_id);
          debug("        n %d\n", n);

          if (1 + n * 2 > l)
            goto error;

          sc++;
          l--;

          for (k = 0; k < n; k++, sc += 2, l -= 2) {
            tm_id = sc[0] >> 6;

            debug("            **********************\n");
            debug("            tm_id %d (%s)\n", tm_id,
                   ((char *[]){"MSC stream audio",
                               "MSC stream data",
                               "reserved",
                               "MSC packet data"})[tm_id]);

            ff->service[ff->n-1].tm[k].tm_id = tm_id;

            switch (tm_id) {
            case 0:
              asct = sc[0] & 0x3f;
              scid = sc[1] >> 2;
              ps = (sc[1] >> 1) & 1;
              ca_flag = sc[1] & 1;

              ff->service[ff->n-1].tm[k].asct = asct;

              debug("            asct %d\n", asct);
              debug("            scid %d\n", scid);
              debug("            ps %d\n", ps);
              debug("            ca_flag %d\n", ca_flag);
              break;
            case 1:
              dsct = sc[0] & 0x3f;
              scid = sc[1] >> 2;
              ps = (sc[1] >> 1) & 1;
              ca_flag = sc[1] & 1;

              ff->service[ff->n-1].tm[k].dsct = dsct;

              debug("            dsct %d\n", dsct);
              debug("            scid %d\n", scid);
              debug("            ps %d\n", ps);
              debug("            ca_flag %d\n", ca_flag);
              break;
            case 2:
              goto error;
            case 3:
              scid = ((sc[0] & 0x3f) << 6) | (sc[1] >> 2);
              ps = (sc[1] >> 1) & 1;
              ca_flag = sc[1] & 1;

              debug("            scid %d\n", scid);
              debug("            ps %d\n", ps);
              debug("            ca_flag %d\n", ca_flag);
              break;
            }

            ff->service[ff->n-1].tm[k].scid = scid;
            ff->service[ff->n-1].tm[k].ps = ps;
            ff->service[ff->n-1].tm[k].ca_flag = ca_flag;
          }
        }
        break;
      }
      default:
        warn("unhandled fig 0 extension %d\n", extension);
        figs->size--;
        break;
      }

      break;
    }
    case 1: {
      unsigned char *sc = &cur[1];
      int l = len - 1;
      int charset    = cur[0] >> 4;
      int rfu        = (cur[0] >> 3) & 1;
      int extension  = cur[0] & 7;

      f->type = FIG1;
      f->u.fig1.charset = charset;
      f->u.fig1.extension = extension;

      debug("    fig 1 charset    %d\n", charset);
      debug("    fig 1 rfu        %d\n", rfu);
      debug("    fig 1 extension  %d\n", extension);

      switch (extension) {
      case 0: {
        fig1_0 *ff = &f->u.fig1_0;
        int identifier;
        if (l < 2) goto error;
        f->type = FIG1_0;
        identifier = (sc[0] << 8) | sc[1];
        ff->country_id = identifier >> 12;
        ff->ensemble_reference = identifier & 0xfff;
        l -= 2;
        sc += 2;

        debug("    fig 1 identifier %d (%4.4x) (country id %d ensemble ref %d)\n", identifier, identifier, identifier >> 12, identifier & 0xfff);

        break;
      }
      case 1: {
        fig1_1 *ff = &f->u.fig1_1;
        int identifier;
        if (l < 2) goto error;
        f->type = FIG1_1;
        identifier = (sc[0] << 8) | sc[1];
        ff->country_id = identifier >> 12;
        ff->service_reference = identifier & 0xfff;
        l -= 2;
        sc += 2;

        debug("    fig 1 identifier %d (%4.4x) (country id %d service ref %d)\n", identifier, identifier, identifier >> 12, identifier & 0xfff);

        break;
      }
      case 4: {
        fig1_4 *ff = &f->u.fig1_4;
        if (l < 3 || ((sc[0] & 0x80) && l < 5)) goto error;
        f->type = FIG1_4;
        ff->pd = sc[0] >> 7;
        ff->scids = sc[0] & 0xf;

        debug("    fig 1 pd %d\n", ff->pd);
        debug("    fig 1 scids %d\n", ff->scids);

        if (ff->pd) {
          ff->ecc = sc[1];
          ff->country_id = sc[2] >> 4;
          ff->service_reference = ((sc[2] & 0xf) << 16) | (sc[3] << 8) | sc[4];
        } else {
          ff->country_id = sc[1] >> 4;
          ff->service_reference = ((sc[1] & 0xf) << 8) | sc[2];
        }
        if (ff->pd) {
          l -= 5;
          sc += 5;
        } else {
          l -= 3;
          sc += 3;
        }

        if (ff->pd) debug("    fig 1 ecc %d country id %d service reference %d\n", ff->ecc, ff->country_id, ff->service_reference);
        else debug("    fig 1 country id %d service reference %d\n", ff->country_id, ff->service_reference);

        break;
      }
      default:
        warn("unhandled fig 1 extension %d\n", extension);
        figs->size--;
        goto eof_fig1;
      }

      if (l < 16 + 2) goto error;
      if (l != 16 + 2) warn("bad length (%d) for FIG 1/0\n", len);

      for (i = 0; i < 16; i++)
        f->u.fig1.charfield[i] = sc[i];

      f->u.fig1.char_flag_field = (sc[16] << 8) | sc[17];

      debug("    fig 1 charfield  '");
      for (i = 0; i < 16 && f->u.fig1.charfield[i]; i++)
        debug("%c", f->u.fig1.charfield[i]);
      debug("'\n");

      debug("    fig 1 char_flag_field 0x%x\n", f->u.fig1.char_flag_field);

eof_fig1:
      break;
    }
    default:
      warn("unhandled fig id %d\n", id);
      figs->size--;
      break;
    }

    cur += len;
    processed += len;
  }

  return 0;

error:
  error("bad fib:");
  for (i = 0; i < 30; i++)
    Error(" %2.2x", fib[i]);
  Error("\n");
  figs->size = figs_size;
  return 1;
}
