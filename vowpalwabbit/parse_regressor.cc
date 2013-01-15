/*
Copyright (c) by respective owners including Yahoo!, Microsoft, and
individual contributors. All rights reserved.  Released under a BSD (revised)
license as described in the file LICENSE.
 */
#include <fstream>
#include <iostream>
using namespace std;

#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "parse_regressor.h"
#include "loss_functions.h"
#include "global_data.h"
#include "io.h"
#include "rand48.h"

/* Define the last version where files are backward compatible. */
#define LAST_COMPATIBLE_VERSION "6.1.3"
#define VERSION_FILE_WITH_CUBIC "6.1.3"

void initialize_regressor(vw& all)
{
  size_t length = ((size_t)1) << all.num_bits;
  all.weight_mask = (all.stride * length) - 1;
  all.reg.weight_vector = (weight *)calloc(all.stride*length, sizeof(weight));
  if (all.reg.weight_vector == NULL)
    {
      cerr << all.program_name << ": Failed to allocate weight array with " << all.num_bits << " bits: try decreasing -b <bits>" << endl;
      exit (1);
    }
  if (all.random_weights)
    {
      for (size_t j = 0; j < length; j++)
	all.reg.weight_vector[j*all.stride] = (float)(frand48() - 0.5);
    }
  if (all.initial_weight != 0.)
    for (size_t j = 0; j < all.stride*length; j+=all.stride)
      all.reg.weight_vector[j] = all.initial_weight;
}

void free_regressor(regressor &r)
{
  if (r.weight_vector != NULL)
    free(r.weight_vector);
}

const size_t buf_size = 512;

void save_load_header(vw& all, io_buf& model_file, bool read, bool text)
{
  char buff[buf_size];
  char buff2[buf_size];
  size_t text_len;

  if (model_file.files.size() > 0)
    {
      uint32_t v_length = version.to_string().length()+1;
      text_len = sprintf(buff, "Version %s\n", version.to_string().c_str());
      memcpy(buff2,version.to_string().c_str(),v_length);
      if (read)
	v_length = buf_size;
      bin_text_read_write(model_file, buff2, v_length, 
			  "", read, 
			  buff, text_len, text);
      version_struct v_tmp(buff2);
      if (v_tmp < LAST_COMPATIBLE_VERSION)
	{
	  cout << "Model has possibly incompatible version! " << v_tmp.to_string() << endl;
	  exit(1);
	}
      
      char model = 'm';
      bin_text_read_write_fixed(model_file,&model,1,
				"file is not a model file", read, 
				"", 0, text);
      
      text_len = sprintf(buff, "Min label:%f\n", all.sd->min_label);
      bin_text_read_write_fixed(model_file,(char*)&all.sd->min_label, sizeof(all.sd->min_label), 
				"", read, 
				buff, text_len, text);
      
      text_len = sprintf(buff, "Max label:%f\n", all.sd->max_label);
      bin_text_read_write_fixed(model_file,(char*)&all.sd->max_label, sizeof(all.sd->max_label), 
				"", read, 
				buff, text_len, text);
      
      text_len = sprintf(buff, "bits:%d\n", (int)all.num_bits);
      uint32_t local_num_bits = all.num_bits;
      bin_text_read_write_fixed(model_file,(char *)&local_num_bits, sizeof(local_num_bits), 
				"", read, 
				buff, text_len, text);
      if (all.default_bits != true && all.num_bits != local_num_bits)
	{
	  cout << "Wrong number of bits for source!" << endl;
	  exit (1);
	}
      all.default_bits = false;
      all.num_bits = local_num_bits;
      
      uint32_t pair_len = all.pairs.size();
      text_len = sprintf(buff, "%d pairs: ", (int)pair_len);
      bin_text_read_write_fixed(model_file,(char *)&pair_len, sizeof(pair_len), 
				"", read, 
				buff, text_len, text);
      for (size_t i = 0; i < pair_len; i++)
	{
	  char pair[2];
	  if (!read)
	    {
	      memcpy(pair,all.pairs[i].c_str(),2);
	      text_len = sprintf(buff, "%s ", all.pairs[i].c_str());
	    }
	  bin_text_read_write_fixed(model_file, pair,2, 
				    "", read,
				    buff, text_len, text);
	  if (read)
	    {
	      string temp(pair, 2);
	      all.pairs.push_back(temp);
	    }
	}
      bin_text_read_write_fixed(model_file,buff,0,
				"", read,
				"\n",1,text);
      
      uint32_t triple_len = all.triples.size();
      text_len = sprintf(buff, "%d triples: ", (int)triple_len);
      bin_text_read_write_fixed(model_file,(char *)&triple_len, sizeof(triple_len), 
				"", read, 
			    buff,text_len, text);
      for (size_t i = 0; i < triple_len; i++)
	{
	  text_len = sprintf(buff, "%s ", all.triples[i].c_str());
	  char triple[3];
	  if (!read)
	    memcpy(triple, all.triples[i].c_str(), 3);
	  bin_text_read_write_fixed(model_file,triple,3, 
				    "", read,
				    buff,text_len,text);
	  if (read)
	    {
	      string temp(triple,3);
	      all.triples.push_back(temp);
	    }
	}
      bin_text_read_write_fixed(model_file,buff,0,
				"", read, 
				"\n",1, text);
      
      text_len = sprintf(buff, "rank:%d\n", (int)all.rank);
      bin_text_read_write_fixed(model_file,(char*)&all.rank, sizeof(all.rank), 
				"", read, 
				buff,text_len, text);
      
      text_len = sprintf(buff, "lda:%d\n", (int)all.lda);
      bin_text_read_write_fixed(model_file,(char*)&all.lda, sizeof(all.lda), 
				"", read, 
				buff, text_len,text);
      
      text_len = sprintf(buff, "ngram:%d\n", (int)all.ngram); 
      bin_text_read_write_fixed(model_file,(char*)&all.ngram, sizeof(all.ngram), 
				"", read, 
				buff, text_len, text);
      
      text_len = sprintf(buff, "skips:%d\n", (int)all.skips);
      bin_text_read_write_fixed(model_file,(char*)&all.skips, sizeof(all.skips), 
				"", read, 
				buff, text_len, text);
      
      text_len = sprintf(buff, "options:%s\n", all.options_from_file.c_str());
      uint32_t len = all.options_from_file.length()+1;
      memcpy(buff2, all.options_from_file.c_str(),len);
      if (read)
	len = buf_size;
      bin_text_read_write(model_file,buff2, len, 
			  "", read,
			  buff, text_len, text);
      if (read)
	all.options_from_file.assign(buff2);
    }

}

void dump_regressor(vw& all, string reg_name, bool as_text)
{
  if (reg_name == string(""))
    return;
  string start_name = reg_name+string(".writing");
  io_buf io_temp;

  io_temp.open_file(start_name.c_str(), all.stdin_off, io_buf::WRITE);
  
  save_load_header(all, io_temp, false, as_text);
  all.save_load(&all, io_temp, false, as_text);

  io_temp.flush(); // close_file() should do this for me ...
  io_temp.close_file();
  remove(reg_name.c_str());
  rename(start_name.c_str(),reg_name.c_str());
}

void save_predictor(vw& all, string reg_name, size_t current_pass)
{
  char* filename = new char[reg_name.length()+4];
  if (all.save_per_pass)
    sprintf(filename,"%s.%lu",reg_name.c_str(),(long unsigned)current_pass);
  else
    sprintf(filename,"%s",reg_name.c_str());
  dump_regressor(all, string(filename), false);
  delete[] filename;
}

void finalize_regressor(vw& all, string reg_name)
{
  if (all.per_feature_regularizer_output.length() > 0)
    dump_regressor(all, all.per_feature_regularizer_output, false);
  else
    dump_regressor(all, reg_name, false);
  if (all.per_feature_regularizer_text.length() > 0)
    dump_regressor(all, all.per_feature_regularizer_text, true);
  else
    dump_regressor(all, all.text_regressor_name, true);
  free_regressor(all.reg);
}

void parse_regressor_args(vw& all, po::variables_map& vm, io_buf& io_temp)
{
  if (vm.count("final_regressor")) {
    all.final_regressor_name = vm["final_regressor"].as<string>();
    if (!all.quiet)
      cerr << "final_regressor = " << vm["final_regressor"].as<string>() << endl;
  }
  else
    all.final_regressor_name = "";

  vector<string> regs;
  if (vm.count("initial_regressor") || vm.count("i"))
    regs = vm["initial_regressor"].as< vector<string> >();
  
  if (regs.size() > 0)
    io_temp.open_file(regs[0].c_str(), all.stdin_off, io_buf::READ);

  save_load_header(all, io_temp, true, false);
}

