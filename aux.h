#ifndef aux_h
#define aux_h

#include "sector_list.h"

typedef char * sstring;

int sstring_create(sstring *, char *, size_t, struct sector_list *);
int sstring_create2(sstring *, struct string *, struct sector_list *);
void sstring_destroy(sstring);
size_t sstring_size(sstring);

#endif
