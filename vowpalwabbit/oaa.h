#ifndef OAA_H
#define OAA_H

#include "io.h"
#include "parse_primitives.h"
#include "global_data.h"
#include "example.h"

struct oaa_data {
  uint32_t label;
  float weight;
};

char* bufread_oaa_label(oaa_data* ld, char* c);
char* bufcache_oaa_label(oaa_data* ld, char* c);

void parse_oaa_flag(size_t s);
void return_oaa_example(example* ec);
example* get_oaa_example();

size_t read_cached_oaa_label(void* v, io_buf& cache);
void cache_oaa_label(void* v, io_buf& cache);
void default_oaa_label(void* v);
void parse_oaa_label(void* v, v_array<substring>& words);
void delete_oaa_label(void* v);
float oaa_weight(void* v);
float oaa_initial(void* v);
const label_parser oaa_label = {default_oaa_label, parse_oaa_label, 
				cache_oaa_label, read_cached_oaa_label, 
				delete_oaa_label, get_weight, get_initial, 
				sizeof(oaa_data)};

#endif
