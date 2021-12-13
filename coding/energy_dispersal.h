#ifndef _ENERGY_DISPERSAL_H_
#define _ENERGY_DISPERSAL_H_

typedef struct {
  char *v;
  int size;
} energy_dispersal;

energy_dispersal *new_energy_dispersal(int size);
void do_energy_dispersal(energy_dispersal *e, char *in_out);
void do_energy_dispersal_n(energy_dispersal *e, char *in_out, int n);

#endif /* _ENERGY_DISPERSAL_H_ */
