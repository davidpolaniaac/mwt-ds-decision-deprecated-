/*
Copyright (c) by respective owners including Yahoo!, Microsoft, and
individual contributors. All rights reserved.  Released under a BSD (revised)
license as described in the file LICENSE.
 */
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

#include <iostream>
#include <float.h>
#include <stdio.h>
#include <math.h>
#include "searn.h"
#include "gd.h"
#include "io.h"
#include "parser.h"
#include "constant.h"
#include "oaa.h"
#include "csoaa.h"
#include "cb.h"
#include "v_hashmap.h"
#include "vw.h"
#include "rand48.h"

// task-specific includes
#include "searn_sequencetask.h"

namespace SearnUtil
{
  using namespace std;

  string   audit_feature_space("history");
  uint32_t history_constant    = 8290741;
  uint32_t example_number = 0;


  void default_info(history_info* hinfo)
  {
    hinfo->bigrams           = false;
    hinfo->features          = 0;
    hinfo->bigram_features   = false;
    hinfo->length            = 1;
  }

  void* calloc_or_die(size_t nmemb, size_t size)
  {
    if (nmemb == 0 || size == 0)
      return NULL;

    void* data = calloc(nmemb, size);
    if (data == NULL) {
      std::cerr << "internal error: memory allocation failed; dying!" << std::endl;
      exit(-1);
    }
    return data;
  }

  void free_it(void*ptr)
  {
    if (ptr != NULL)
      free(ptr);
  }

  void add_policy_offset(vw&all, example *ec, uint32_t increment, uint32_t policy)
  {
    update_example_indicies(all.audit, ec, policy * increment);
  }

  void remove_policy_offset(vw&all, example *ec, uint32_t increment, uint32_t policy)
  {
    update_example_indicies(all.audit, ec, -policy * increment);
  }

  int random_policy(uint64_t seed, float beta, bool allow_current_policy, int current_policy, bool allow_optimal, bool reset_seed)
  {
    if(reset_seed) //reset_seed is false for contextual bandit, so that we only reset the seed if the base learner is not a contextual bandit learner, as this breaks the exploration.
      msrand48(seed * 2147483647);

    if (beta >= 1) {
      if (allow_current_policy) return (int)current_policy;
      if (current_policy > 0) return (((int)current_policy)-1);
      if (allow_optimal) return -1;
      std::cerr << "internal error (bug): no valid policies to choose from!  defaulting to current" << std::endl;
      return (int)current_policy;
    }
    
    int num_valid_policies = (int)current_policy + allow_optimal + allow_current_policy;
    int pid = -1;
    
    if (num_valid_policies == 0) {
      std::cerr << "internal error (bug): no valid policies to choose from!  defaulting to current" << std::endl;
      return (int)current_policy;
    } else if (num_valid_policies == 1) {
      pid = 0;
    } else {
      float r = frand48();
      pid = 0;
      if (r > beta) {
        r -= beta;
        while ((r > 0) && (pid < num_valid_policies-1)) {
          pid ++;
          r -= beta * powf(1.f - beta, (float)pid);
        }
      }
    }

    // figure out which policy pid refers to
    if (allow_optimal && (pid == num_valid_policies-1))
      return -1; // this is the optimal policy
  
    pid = (int)current_policy - pid;
    if (!allow_current_policy)
      pid--;

    return pid;
  }

  void add_history_to_example(vw&all, history_info *hinfo, example* ec, history h)
  {
    size_t v0, v, max_string_length = 0;
    size_t total_length = max(hinfo->features, hinfo->length);

    if (total_length == 0) return;
    if (h == NULL) {
      cerr << "error: got empty history in add_history_to_example" << endl;
      exit(-1);
    }

    if (all.audit) {
      max_string_length = max((int)(ceil( log10((float)total_length+1) )),
                              (int)(ceil( log10((float)MAX_ACTION_ID+1) ))) + 1;
    }

    for (size_t t=1; t<=total_length; t++) {
      v0 = (h[hinfo->length-t] * quadratic_constant + t) * quadratic_constant + history_constant;

      // add the basic history features
      feature temp = {1., (uint32_t) ( (2*v0) & all.parse_mask )};
      ec->atomics[history_namespace].push_back(temp);

      if (all.audit) {
        audit_data a_feature = { NULL, NULL, (uint32_t)((2*v0) & all.parse_mask), 1., true };
        a_feature.space = (char*)calloc_or_die(audit_feature_space.length()+1, sizeof(char));
        strcpy(a_feature.space, audit_feature_space.c_str());

        a_feature.feature = (char*)calloc_or_die(5 + 2*max_string_length, sizeof(char));
        sprintf(a_feature.feature, "ug@%d=%d", (int)t, (int)h[hinfo->length-t]);

        ec->audit_features[history_namespace].push_back(a_feature);
      }

      // add the bigram features
      if ((t > 1) && hinfo->bigrams) {
        v0 = ((v0 - history_constant) * quadratic_constant + h[hinfo->length-t+1]) * quadratic_constant + history_constant;

        feature temp = {1., (uint32_t) ( (2*v0) & all.parse_mask )};
        ec->atomics[history_namespace].push_back(temp);

        if (all.audit) {
          audit_data a_feature = { NULL, NULL, (uint32_t)((2*v0) & all.parse_mask), 1., true };
          a_feature.space = (char*)calloc_or_die(audit_feature_space.length()+1, sizeof(char));
          strcpy(a_feature.space, audit_feature_space.c_str());

          a_feature.feature = (char*)calloc_or_die(6 + 3*max_string_length, sizeof(char));
          sprintf(a_feature.feature, "bg@%d=%d-%d", (int)t-1, (int)h[hinfo->length-t], (int)h[hinfo->length-t+1]);

          ec->audit_features[history_namespace].push_back(a_feature);
        }

      }
    }

    string fstring;

    if (hinfo->features > 0) {
      for (unsigned char* i = ec->indices.begin; i != ec->indices.end; i++) {
        int feature_index = 0;
        for (feature* f = ec->atomics[*i].begin; f != ec->atomics[*i].end; f++) {

          if (all.audit) {
            if (feature_index >= (int)ec->audit_features[*i].size() ) {
              char buf[32];
              sprintf(buf, "{%d}", f->weight_index);
              fstring = string(buf);
            } else 
              fstring = string(ec->audit_features[*i][feature_index].feature);
            feature_index++;
          }

          v = f->weight_index + history_constant;

          for (size_t t=1; t<=hinfo->features; t++) {
            v0 = (h[hinfo->length-t] * quadratic_constant + t) * quadratic_constant;
          
            // add the history/feature pair
            feature temp = {1., (uint32_t) ( (2*(v0 + v)) & all.parse_mask )};
            ec->atomics[history_namespace].push_back(temp);

            if (all.audit) {
              audit_data a_feature = { NULL, NULL, (uint32_t)((2*(v+v0)) & all.parse_mask), 1., true };
              a_feature.space = (char*)calloc_or_die(audit_feature_space.length()+1, sizeof(char));
              strcpy(a_feature.space, audit_feature_space.c_str());

              a_feature.feature = (char*)calloc_or_die(8 + 2*max_string_length + fstring.length(), sizeof(char));
              sprintf(a_feature.feature, "ug+f@%d=%d=%s", (int)t, (int)h[hinfo->length-t], fstring.c_str());

              ec->audit_features[history_namespace].push_back(a_feature);
            }


            // add the bigram
            if ((t > 0) && hinfo->bigram_features) {
              v0 = (v0 + h[hinfo->length-t+1]) * quadratic_constant;

              feature temp = {1., (uint32_t) ( (2*(v + v0)) & all.parse_mask )};
              ec->atomics[history_namespace].push_back(temp);

              if (all.audit) {
                audit_data a_feature = { NULL, NULL, (uint32_t)((2*(v+v0)) & all.parse_mask), 1., true };
                a_feature.space = (char*)calloc_or_die(audit_feature_space.length()+1, sizeof(char));
                strcpy(a_feature.space, audit_feature_space.c_str());

                a_feature.feature = (char*)calloc_or_die(9 + 3*max_string_length + fstring.length(), sizeof(char));
                sprintf(a_feature.feature, "bg+f@%d=%d-%d=%s", (int)t-1, (int)h[hinfo->length-t], (int)h[hinfo->length-t+1], fstring.c_str());

                ec->audit_features[history_namespace].push_back(a_feature);
              }

            }
          }
        }
      }
    }

    ec->indices.push_back(history_namespace);
    ec->sum_feat_sq[history_namespace] += ec->atomics[history_namespace].size();
    ec->total_sum_feat_sq += ec->sum_feat_sq[history_namespace];
    ec->num_features += ec->atomics[history_namespace].size();
  }

  void remove_history_from_example(vw&all, history_info *hinfo, example* ec)
  {
    size_t total_length = max(hinfo->features, hinfo->length);
    if (total_length == 0) return;

    if (ec->indices.size() == 0) {
      cerr << "internal error (bug): trying to remove history, but there are no namespaces!" << endl;
      return;
    }

    if (ec->indices.last() != history_namespace) {
      cerr << "internal error (bug): trying to remove history, but either it wasn't added, or something was added after and not removed!" << endl;
      return;
    }

    ec->num_features -= ec->atomics[history_namespace].size();
    ec->total_sum_feat_sq -= ec->sum_feat_sq[history_namespace];
    ec->sum_feat_sq[history_namespace] = 0;
    ec->atomics[history_namespace].erase();
    if (all.audit) {
      if (ec->audit_features[history_namespace].begin != ec->audit_features[history_namespace].end) {
        for (audit_data *f = ec->audit_features[history_namespace].begin; f != ec->audit_features[history_namespace].end; f++) {
          if (f->alloced) {
            free(f->space);
            free(f->feature);
            f->alloced = false;
          }
        }
      }

      ec->audit_features[history_namespace].erase();
    }
    ec->indices.decr();
  }

}

namespace Searn
{
  // task stuff
  search_task task;
  bool is_singleline;
  bool is_ldf;
  bool has_hash;
  bool constrainted_actions;
  size_t input_label_size;

  // options
  size_t max_action           = 1;
  size_t max_rollout          = INT_MAX;
  size_t passes_per_policy    = 1;     //this should be set to the same value as --passes for dagger
  float  beta                 = 0.5;
  float gamma                = 1.;
  bool   do_recombination     = false;
  bool   allow_current_policy = false; //this should be set to true for dagger
  bool   rollout_oracle       = false; //if true then rollout are performed using oracle instead (optimal approximation discussed in searn's paper). this should be set to true for dagger
  bool   adaptive_beta        = false; //used to implement dagger through searn. if true, beta = 1-(1-alpha)^n after n updates, and policy is mixed with oracle as \pi' = (1-beta)\pi^* + beta \pi
  float  alpha                = 0.001f; //parameter used to adapt beta for dagger (see above comment), should be in (0,1)
  bool   rollout_all_actions  = true;  //by default we rollout all actions. This is set to false when searn is used with a contextual bandit base learner, where we rollout only one sampled action

  // debug stuff
  bool PRINT_DEBUG_INFO             = 0;
  bool PRINT_UPDATE_EVERY_EXAMPLE   = 0 | PRINT_DEBUG_INFO;


  // rollout
  struct rollout_item {
    state st;
    bool  is_finished;
    bool  alive;
    size_t hash;
  };

  // memory
  rollout_item* rollout;
  v_array<example*> ec_seq = v_array<example*>();
  example** global_example_set = NULL;
  example* empty_example = NULL;
  OAA::mc_label empty_label;
  v_array<CSOAA::wclass>loss_vector = v_array<CSOAA::wclass>();
  v_array<CB::cb_class>loss_vector_cb = v_array<CB::cb_class>();
  v_array<void*>old_labels = v_array<void*>();
  v_array<OAA::mc_label>new_labels = v_array<OAA::mc_label>();
  CSOAA::label testall_labels = { v_array<CSOAA::wclass>() };
  CSOAA::label allowed_labels = { v_array<CSOAA::wclass>() };
  CB::label testall_labels_cb = { v_array<CB::cb_class>() };
  CB::label allowed_labels_cb = { v_array<CB::cb_class>() };

  // we need a hashmap that maps from STATES to ACTIONS
  v_hashmap<state,action> *past_states = NULL;
  v_array<state> unfreed_states = v_array<state>();

  // tracking of example
  size_t read_example_this_loop   = 0;
  size_t read_example_last_id     = 0;
  size_t passes_since_new_policy  = 0;
  size_t read_example_last_pass   = 0;
  size_t total_examples_generated = 0;
  size_t total_predictions_made   = 0;
  size_t searn_num_features       = 0;

  // variables
  uint32_t current_policy           = 0;
  uint32_t total_number_of_policies = 1;
  uint32_t increment                = 0; //for policy offset

  void (*base_learner)(void*, example*) = NULL;
  void (*base_finish)(void*) = NULL;

  void simple_print_example_features(vw&all, example *ec)
  {
    for (unsigned char* i = ec->indices.begin; i != ec->indices.end; i++) 
      {
        feature* end = ec->atomics[*i].end;
        for (feature* f = ec->atomics[*i].begin; f!= end; f++) {
          cerr << "\t" << f->weight_index << ":" << f->x << ":" << all.reg.weight_vector[f->weight_index & all.weight_mask];
        }
      }
    cerr << endl;
  }

  void simple_print_costs(CSOAA::label *c)
  {
    for (CSOAA::wclass *f = c->costs.begin; f != c->costs.end; f++) {
      clog << "\t" << f->weight_index << ":" << f->x << "::" << f->partial_prediction;
    }
    clog << endl;
  }

  bool should_print_update(vw& all)
  {
    //uncomment to print out final loss after all examples processed
    //commented for now so that outputs matches make test
    //if( parser_done(all.p)) return true;

    if (!(all.sd->weighted_examples > all.sd->dump_interval && !all.quiet && !all.bfgs)) {
      if (!PRINT_UPDATE_EVERY_EXAMPLE) return false;
    }
    return true;
  }

  std::vector<action> empty_action_vector = std::vector<action>();

  void to_short_string(string in, size_t max_len, char*out) {
    for (size_t i=0; i<max_len; i++) {
      if (i >= in.length())
        out[i] = ' ';
      else if ((in[i] == '\n') || (in[i] == '\t'))    // TODO: maybe catch other characters?
        out[i] = ' ';
      else
        out[i] = in[i];
    }

    if (in.length() > max_len) { 
      out[max_len-2] = '.'; 
      out[max_len-1] = '.'; 
    }
    out[max_len] = 0;
  }

  bool will_global_print_label(vw& all)
  {
    if (!task.to_string) return false;
    if (all.final_prediction_sink.size() == 0) return false;
    return true;
  }

  void global_print_label(vw& all, example*ec, state s0, std::vector<action> last_action_sequence)
  {
    if (!task.to_string) return;
    if (all.final_prediction_sink.size() == 0) return;

    string str = task.to_string(s0, false, last_action_sequence);
    for (size_t i=0; i<all.final_prediction_sink.size(); i++) {
      int f = all.final_prediction_sink[i];
      all.print_text(f, str, ec->tag);
    }
  }

  void print_update(vw& all, state s0, std::vector<action> last_action_sequence)
  {
    if (!should_print_update(all))
      return;

    char true_label[21];
    char pred_label[21];
    if (task.to_string) {
      to_short_string(task.to_string(s0, true , empty_action_vector ), 20, true_label);
      to_short_string(task.to_string(s0, false, last_action_sequence), 20, pred_label);
    } else {
      to_short_string("", 20, true_label);
      to_short_string("", 20, pred_label);
    }

    fprintf(stderr, "%-10.6f %-10.6f %8ld %15f   [%s] [%s] %8lu %5d %5d %15lu %15lu\n",
            all.sd->sum_loss/all.sd->weighted_examples,
            all.sd->sum_loss_since_last_dump / (all.sd->weighted_examples - all.sd->old_weighted_examples),
            (long int)all.sd->example_number,
            all.sd->weighted_examples,
            true_label,
            pred_label,
            (long unsigned int)searn_num_features,
            (int)read_example_last_pass,
            (int)current_policy,
            (long unsigned int)total_predictions_made,
            (long unsigned int)total_examples_generated);

    all.sd->sum_loss_since_last_dump = 0.0;
    all.sd->old_weighted_examples = all.sd->weighted_examples;
    all.sd->dump_interval *= 2;
  }



  void clear_seq(vw&all)
  {
    if (ec_seq.size() > 0) 
      for (example** ecc=ec_seq.begin; ecc!=ec_seq.end; ecc++) {
	VW::finish_example(all, *ecc);
      }
    ec_seq.erase();
  }

  void free_unfreed_states()
  {
    while (!unfreed_states.empty()) {
      state s = unfreed_states.pop();
      task.finish(s);
    }
  }

  void initialize_memory()
  {
    // initialize searn's memory
    rollout = (rollout_item*)SearnUtil::calloc_or_die(max_action, sizeof(rollout_item));
    global_example_set = (example**)SearnUtil::calloc_or_die(max_action, sizeof(example*));

    for (uint32_t k=1; k<=max_action; k++) {
      CSOAA::wclass cost = { FLT_MAX, k, 1., 0. };
      testall_labels.costs.push_back(cost);
      CB::cb_class cost_cb = { FLT_MAX, k, 0. };
      testall_labels_cb.costs.push_back(cost_cb);
    }

    empty_example = alloc_example(sizeof(OAA::mc_label));
    OAA::default_label(empty_example->ld);
    //    cerr << "create: empty_example->ld = " << empty_example->ld << endl;
    empty_example->in_use = true;
  }
  
  void free_memory(vw&all)
  {
    dealloc_example(NULL, *empty_example);
    free(empty_example);

    SearnUtil::free_it(rollout);

    loss_vector.delete_v();

    old_labels.delete_v();

    new_labels.delete_v();

    free_unfreed_states();
    unfreed_states.delete_v();

    clear_seq(all);
    ec_seq.delete_v();

    SearnUtil::free_it(global_example_set);

    testall_labels.costs.delete_v();
    testall_labels_cb.costs.delete_v();
    allowed_labels.costs.delete_v();
    allowed_labels_cb.costs.delete_v();

    if (do_recombination) {
      delete past_states;
      past_states = NULL;
    }
  }



  void learn(void*in, example *ec)
  {
    //vw*all = (vw*)in;
    // TODO
  }

  void finish(void*in)
  {
    vw*all = (vw*)in;
    // free everything
    if (task.finalize != NULL)
      task.finalize();
    free_memory(*all);
    base_finish(all);
  }

  void parse_flags(vw&all, std::vector<std::string>&opts, po::variables_map& vm, po::variables_map& vm_file)
  {
    po::options_description desc("Searn options");
    desc.add_options()
      ("searn_task", po::value<string>(), "the searn task")
      ("searn_rollout", po::value<size_t>(), "maximum rollout length")
      ("searn_passes_per_policy", po::value<size_t>(), "maximum number of datapasses per policy")
      ("searn_beta", po::value<float>(), "interpolation rate for policies")
      ("searn_gamma", po::value<float>(), "discount rate for policies")
      ("searn_recombine", "allow searn labeling to use the current policy")
      ("searn_allow_current_policy", "allow searn labeling to use the current policy")
      ("searn_rollout_oracle", "allow searn/dagger to do rollouts with the oracle when estimating cost-to-go")
      ("searn_as_dagger", po::value<float>(), "sets options to make searn operate as dagger. parameter is the sliding autonomy rate (rate at which beta tends to 1).")
      ("searn_total_nb_policies", po::value<size_t>(), "if we are going to train the policies through multiple separate calls to vw, we need to specify this parameter and tell vw how many policies are eventually going to be trained");

    po::options_description add_desc_file("Searn options only available in regressor file");
    add_desc_file.add_options()
      ("searn_trained_nb_policies", po::value<size_t>(), "the number of trained policies in the regressor file");

    po::options_description desc_file;
    desc_file.add(desc).add(add_desc_file);

    po::parsed_options parsed = po::command_line_parser(opts).
      style(po::command_line_style::default_style ^ po::command_line_style::allow_guessing).
      options(desc).allow_unregistered().run();
    opts = po::collect_unrecognized(parsed.options, po::include_positional);
    po::store(parsed, vm);
    po::notify(vm);

    po::parsed_options parsed_file = po::command_line_parser(all.options_from_file_argc, all.options_from_file_argv).
      style(po::command_line_style::default_style ^ po::command_line_style::allow_guessing).
      options(desc_file).allow_unregistered().run();
    po::store(parsed_file, vm_file);
    po::notify(vm_file);
  
    std::string task_string;
    if(vm_file.count("searn_task")) {//we loaded searn task flag from regressor file    
      task_string = vm_file["searn_task"].as<std::string>();
      if(vm.count("searn_task") && task_string.compare(vm["searn_task"].as<std::string>()) != 0 )
      {
        std::cerr << "warning: specified --searn_task different than the one loaded from regressor. Pursuing with loaded value of: " << task_string << endl;
      }
    }
    else {
      if (vm.count("searn_task") == 0) {
        cerr << "must specify --searn_task" << endl;
        exit(-1);
      }
      task_string = vm["searn_task"].as<std::string>();

      //append the searn task to options_from_file so it is saved in the regressor file later
      all.options_from_file.append(" --searn_task ");
      all.options_from_file.append(task_string);
    }

    if (task_string.compare("sequence") == 0) {
      task.final = SequenceTask::final;
      task.loss = SequenceTask::loss;
      task.step = SequenceTask::step; 
      task.oracle = SequenceTask::oracle;
      task.copy = SequenceTask::copy;
      task.finish = SequenceTask::finish; 
      task.searn_label_parser = OAA::mc_label_parser;
      task.is_test_example = SequenceTask::is_test_example;
      input_label_size = sizeof(OAA::mc_label);
      task.start_state = NULL;
      task.start_state_multiline = SequenceTask::start_state_multiline;
      if (1) {
        task.cs_example = SequenceTask::cs_example;
        task.cs_ldf_example = NULL;
      } else {
        task.cs_example = NULL;
        task.cs_ldf_example = SequenceTask::cs_ldf_example;
      }
      task.initialize = SequenceTask::initialize;
      task.finalize = NULL;
      task.equivalent = SequenceTask::equivalent;
      task.hash = SequenceTask::hash;
      task.allowed = SequenceTask::allowed;
      task.to_string = SequenceTask::to_string;
    } else {
      std::cerr << "error: unknown search task '" << task_string << "'" << std::endl;
      exit(-1);
    }

    *(all.p->lp)=task.searn_label_parser;

    if(vm_file.count("searn")) { //we loaded searn flag from regressor file 
      max_action = vm_file["searn"].as<size_t>();
      if( vm.count("searn") && vm["searn"].as<size_t>() != max_action )
        std::cerr << "warning: you specified a different number of actions through --searn than the one loaded from predictor. Pursuing with loaded value of: " << max_action << endl;
    }
    else {
      max_action = vm["searn"].as<size_t>();

      //append searn with nb_actions to options_from_file so it is saved to regressor later
      std::stringstream ss;
      ss << " --searn " << max_action;
      all.options_from_file.append(ss.str());
    }

    if(vm_file.count("searn_beta")) { //we loaded searn_beta flag from regressor file 
      beta = vm_file["searn_beta"].as<float>();
      if (vm.count("searn_beta") && vm["searn_beta"].as<float>() != beta )
        std::cerr << "warning: you specified a different value through --searn_beta than the one loaded from predictor. Pursuing with loaded value of: " << beta << endl;

    }
    else {
      if (vm.count("searn_beta")) beta = vm["searn_beta"].as<float>();

      //append searn_beta to options_from_file so it is saved in the regressor file later
      std::stringstream ss;
      ss << " --searn_beta " << beta;
      all.options_from_file.append(ss.str());
    }

    if (vm.count("searn_rollout"))                 max_rollout          = vm["searn_rollout"].as<size_t>();
    if (vm.count("searn_passes_per_policy"))       passes_per_policy    = vm["searn_passes_per_policy"].as<size_t>();
      
    if (vm.count("searn_gamma"))                   gamma                = vm["searn_gamma"].as<float>();
    if (vm.count("searn_norecombine"))             do_recombination     = false;
    if (vm.count("searn_allow_current_policy"))    allow_current_policy = true;
    if (vm.count("searn_rollout_oracle"))    	   rollout_oracle       = true;

    //check if the base learner is contextual bandit, in which case, we dont rollout all actions.
    if ( vm.count("cb") || vm_file.count("cb") ) rollout_all_actions = false;

    //if we loaded a regressor with -i option, --searn_trained_nb_policies contains the number of trained policies in the file
    // and --searn_total_nb_policies contains the total number of policies in the file
    if ( vm_file.count("searn_total_nb_policies") )
    {
      current_policy = (uint32_t)vm_file["searn_trained_nb_policies"].as<size_t>();
      total_number_of_policies = (uint32_t)vm_file["searn_total_nb_policies"].as<size_t>();
      if (vm.count("searn_total_nb_policies") && (uint32_t)vm["searn_total_nb_policies"].as<size_t>() != total_number_of_policies)
          std::cerr << "warning: --searn_total_nb_policies doesn't match the total number of policies stored in initial predictor. Using loaded value of: " << total_number_of_policies << endl;
    }
    else if (vm.count("searn_total_nb_policies"))
    {
      total_number_of_policies = (uint32_t)vm["searn_total_nb_policies"].as<size_t>();
    }

    if (vm.count("searn_as_dagger"))
    {
      //overide previously loaded options to set searn as dagger
      allow_current_policy = true;
      passes_per_policy = all.numpasses;
      //rollout_oracle = true;
      if( current_policy > 1 ) 
        current_policy = 1;

      //indicate to adapt beta for each update
      adaptive_beta = true;
      alpha = vm["searn_as_dagger"].as<float>();
    }

    if (beta <= 0 || beta >= 1) {
      std::cerr << "warning: searn_beta must be in (0,1); resetting to 0.5" << std::endl;
      beta = 0.5;
    }

    if (gamma <= 0 || gamma > 1) {
      std::cerr << "warning: searn_gamma must be in (0,1); resetting to 1.0" << std::endl;
      gamma = 1.0;
    }

    if (alpha < 0 || alpha > 1) {
      std::cerr << "warning: searn_adaptive_beta must be in (0,1); resetting to 0.001" << std::endl;
      alpha = 0.001f;
    }

    if (task.initialize != NULL)
      if (!task.initialize(all, opts, vm, vm_file)) {
        std::cerr << "error: task did not initialize properly" << std::endl;
        exit(-1);
      }

    // check to make sure task is valid and set up our variables
    if (task.final  == NULL ||
        task.loss   == NULL ||
        task.step   == NULL ||
        task.oracle == NULL ||
        task.copy   == NULL ||
        task.finish == NULL ||
        ((task.start_state == NULL) == (task.start_state_multiline == NULL)) ||
        ((task.cs_example  == NULL) == (task.cs_ldf_example        == NULL))) {
      std::cerr << "error: searn task malformed" << std::endl;
      exit(-1);
    }

    is_singleline  = (task.start_state != NULL);
    is_ldf         = (task.cs_example  == NULL);
    has_hash       = (task.hash        != NULL);
    constrainted_actions = (task.allowed != NULL);

    if (do_recombination && (task.hash == NULL)) {
      std::cerr << "warning: cannot do recombination when hashing is unavailable -- turning off recombination" << std::endl;
      do_recombination = false;
    }
    if (do_recombination) {
      // 0 is an invalid action
      past_states = new v_hashmap<state,action>(1023, 0, task.equivalent);
    }

    if (is_ldf && !constrainted_actions) {
      std::cerr << "error: LDF requires allowed" << std::endl;
      exit(-1);
    }

    all.searn = true;

    //compute total number of policies we will have at end of training
    // we add current_policy for cases where we start from an initial set of policies loaded through -i option
    uint32_t tmp_number_of_policies = current_policy; 
    if( all.training )
	tmp_number_of_policies += (int)ceil(((float)all.numpasses) / ((float)passes_per_policy));

    //the user might have specified the number of policies that will eventually be trained through multiple vw calls, 
    //so only set total_number_of_policies to computed value if it is larger
    if( tmp_number_of_policies > total_number_of_policies )
    {
	total_number_of_policies = tmp_number_of_policies;
        if( current_policy > 0 ) //we loaded a file but total number of policies didn't match what is needed for training
        {
          std::cerr << "warning: you're attempting to train more classifiers than was allocated initially. Likely to cause bad performance." << endl;
        }  
    }

    //current policy currently points to a new policy we would train
    //if we are not training and loaded a bunch of policies for testing, we need to subtract 1 from current policy
    //so that we only use those loaded when testing (as run_prediction is called with allow_current to true)
    if( !all.training && current_policy > 0 )
	current_policy--;

    //std::cerr << "Current Policy: " << current_policy << endl;
    //std::cerr << "Total Number of Policies: " << total_number_of_policies << endl;

    std::stringstream ss1;
    std::stringstream ss2;
    ss1 << current_policy;
    //use cmd_string_replace_value in case we already loaded a predictor which had a value stored for --searn_trained_nb_policies
    VW::cmd_string_replace_value(all.options_from_file,"--searn_trained_nb_policies", ss1.str()); 
    ss2 << total_number_of_policies;
    //use cmd_string_replace_value in case we already loaded a predictor which had a value stored for --searn_total_nb_policies
    VW::cmd_string_replace_value(all.options_from_file,"--searn_total_nb_policies", ss2.str());

    all.base_learner_nb_w *= total_number_of_policies;
    increment = ((uint32_t)all.length() / all.base_learner_nb_w) * all.stride;
    //cerr << "searn increment = " << increment << endl;

    all.driver = drive;
    base_learner = all.learn;
    all.learn = learn;
    base_finish = all.finish;
    all.finish = finish;
  }

  uint32_t searn_predict(vw&all, state s0, size_t step, bool allow_oracle, bool allow_current, v_array< pair<uint32_t,float> >* partial_predictions)  // TODO: partial_predictions
  {
    int policy = SearnUtil::random_policy(read_example_last_id * 2147483 + step * 2147483647 /* has_hash ? task.hash(s0) : step */, beta, allow_current, (int)current_policy, allow_oracle, rollout_all_actions);
    if (PRINT_DEBUG_INFO) { cerr << "predicing with policy " << policy << " (allow_oracle=" << allow_oracle << ", allow_current=" << allow_current << "), current_policy=" << current_policy << endl; }
    if (policy == -1) {
      return task.oracle(s0);
    }

    example *ec;

    if (!is_ldf) {
      task.cs_example(all, s0, ec, true);
      SearnUtil::add_policy_offset(all, ec, increment, policy);

      void* old_label = ec->ld;
      if(rollout_all_actions) { //this means we have a cost-sensitive base learner
        ec->ld = (void*)&testall_labels;
        if (task.allowed != NULL) {  // we need to check which actions are allowed
          allowed_labels.costs.erase();
          bool all_allowed = true;
          for (uint32_t k=1; k<=max_action; k++)
            if (task.allowed(s0, k)) {
              CSOAA::wclass cost = { FLT_MAX, k, 1., 0. };
              allowed_labels.costs.push_back(cost);
            } else
              all_allowed = false;

          if (!all_allowed)
            ec->ld = (void*)&allowed_labels;
        }
      }
      else { //if we have a contextual bandit base learner
        ec->ld = (void*)&testall_labels_cb;
        if (task.allowed != NULL) {  // we need to check which actions are allowed
          allowed_labels_cb.costs.erase();
          bool all_allowed = true;
          for (uint32_t k=1; k<=max_action; k++)
            if (task.allowed(s0, k)) {
              CB::cb_class cost = { FLT_MAX, k, 0. };
              allowed_labels_cb.costs.push_back(cost);
            } else
              all_allowed = false;

          if (!all_allowed)
            ec->ld = (void*)&allowed_labels_cb;
        }
      }
      //cerr << "searn>";
      //simple_print_example_features(all,ec);
      base_learner(&all,ec);  
	  total_predictions_made++;  
	  searn_num_features += ec->num_features;
      uint32_t final_prediction = (uint32_t)(*(OAA::prediction_t*)&(ec->final_prediction));
      ec->ld = old_label;

      SearnUtil::remove_policy_offset(all, ec, increment, policy);
      task.cs_example(all, s0, ec, false);

      return final_prediction;
    } else {  // is_ldf
      //TODO: modify this to handle contextual bandit base learner with ldf
      float best_prediction = 0;
      uint32_t best_action = 0;
      for (uint32_t action=1; action <= max_action; action++) {
        if (!task.allowed(s0, action))
          break;   // for LDF, there are no more actions

        task.cs_ldf_example(all, s0, action, ec, true);
        //cerr << "created example: " << ec << ", label: " << ec->ld << endl;
        SearnUtil::add_policy_offset(all, ec, increment, policy);
        base_learner(&all,ec);  total_predictions_made++;  searn_num_features += ec->num_features;
        //cerr << "base_learned on example: " << ec << endl;
        empty_example->in_use = true;
        base_learner(&all,empty_example);
        //cerr << "base_learned on empty example: " << empty_example << endl;
        SearnUtil::remove_policy_offset(all, ec, increment, policy);

        if (action == 1 || 
            ec->partial_prediction < best_prediction) {
          best_prediction = ec->partial_prediction;
          best_action     = action;
        }
        //cerr << "releasing example: " << ec << ", label: " << ec->ld << endl;
        task.cs_ldf_example(all, s0, action, ec, false);
      }

      if (best_action < 1) {
        std::cerr << "warning: internal error on search -- could not find an available action; quitting!" << std::endl;
        exit(-1);
      }
      return best_action;
    }
  }

  float single_rollout(vw&all, state s0, uint32_t action)
  {
    //first check if action is valid for current state
    if( action < 1 || action > max_action || (task.allowed && !task.allowed(s0,action)) )
    {
	std::cerr << "warning: asked to rollout an unallowed action: " << action << "; not performing rollout." << std::endl;
	return 0;
    }
    
    //copy state and step it with current action
    rollout[action-1].alive = true;
    rollout[action-1].st = task.copy(s0);
    task.step(rollout[action-1].st, action);
    rollout[action-1].is_finished = task.final(rollout[action-1].st);
    if (do_recombination) rollout[action-1].hash = task.hash(rollout[action-1].st);

    //if not finished complete rollout
    if (!rollout[action-1].is_finished) {
      for (size_t step=1; step<max_rollout; step++) {
        uint32_t act_tmp = 0;
        if (do_recombination)
          act_tmp = past_states->get(rollout[action-1].st, rollout[action-1].hash);

        if (act_tmp == 0) {  // this means we didn't find it or we're not recombining
          if( !rollout_oracle )
            act_tmp = searn_predict(all, rollout[action-1].st, step, true, allow_current_policy, NULL);
	  else
            act_tmp = task.oracle(rollout[action-1].st);

          if (do_recombination) {
            // we need to make a copy of the state
            state copy = task.copy(rollout[action-1].st);
            past_states->put_after_get(copy, rollout[action-1].hash, act_tmp);
            unfreed_states.push_back(copy);
          }
        }          
          
        task.step(rollout[action-1].st, act_tmp);
        rollout[action-1].is_finished = task.final(rollout[action-1].st);
        if (do_recombination) rollout[action-1].hash = task.hash(rollout[action-1].st);
        if (rollout[action-1].is_finished) break;
      }
    }

    // finally, compute losses and free copies
    float l = task.loss(rollout[action-1].st);
    if ((l == FLT_MAX) && (!rollout[action-1].is_finished) && (max_rollout < INT_MAX)) {
      std::cerr << "error: you asked for short rollouts, but your task does not support pre-final losses" << std::endl;
      exit(-1);
    }
    task.finish(rollout[action-1].st);

    return l;
  }

  void parallel_rollout(vw&all, state s0)
  {
    // first, make K copies of s0 and step them
    bool all_finished = true;
    for (size_t k=1; k<=max_action; k++) 
      rollout[k-1].alive = false;
    
    for (uint32_t k=1; k<=max_action; k++) {
      // in the case of LDF, we might run out of actions early
      if (task.allowed && !task.allowed(s0, k)) {
        if (is_ldf) break;
        else continue;
      }
      rollout[k-1].alive = true;
      rollout[k-1].st = task.copy(s0);
      task.step(rollout[k-1].st, k);
      rollout[k-1].is_finished = task.final(rollout[k-1].st);
      if (do_recombination) rollout[k-1].hash = task.hash(rollout[k-1].st);
      all_finished = all_finished && rollout[k-1].is_finished;
    }

    // now, complete all rollouts
    if (!all_finished) {
      for (size_t step=1; step<max_rollout; step++) {
        all_finished = true;
        for (size_t k=1; k<=max_action; k++) {
          if (rollout[k-1].is_finished) continue;
          
          uint32_t action = 0;
          if (do_recombination)
            action = past_states->get(rollout[k-1].st, rollout[k-1].hash);

          if (action == 0) {  // this means we didn't find it or we're not recombining
            if( !rollout_oracle )
              action = searn_predict(all, rollout[k-1].st, step, true, allow_current_policy, NULL);
	    else
              action = task.oracle(rollout[k-1].st);

            if (do_recombination) {
              // we need to make a copy of the state
              state copy = task.copy(rollout[k-1].st);
              past_states->put_after_get(copy, rollout[k-1].hash, action);
              unfreed_states.push_back(copy);
            }
          }          
          
          task.step(rollout[k-1].st, action);
          rollout[k-1].is_finished = task.final(rollout[k-1].st);
          if (do_recombination) rollout[k-1].hash = task.hash(rollout[k-1].st);
          all_finished = all_finished && rollout[k-1].is_finished;
        }
        if (all_finished) break;
      }
    }

    // finally, compute losses and free copies
    float min_loss = 0;
    loss_vector.erase();
    for (uint32_t k=1; k<=max_action; k++) {
      if (!rollout[k-1].alive)
        break;

      float l = task.loss(rollout[k-1].st);
      if ((l == FLT_MAX) && (!rollout[k-1].is_finished) && (max_rollout < INT_MAX)) {
        std::cerr << "error: you asked for short rollouts, but your task does not support pre-final losses" << std::endl;
        exit(-1);
      }

      CSOAA::wclass temp = { l, k, 1., 0. };
      loss_vector.push_back(temp);
      if ((k == 1) || (l < min_loss)) { min_loss = l; }

      task.finish(rollout[k-1].st);
    }

    // subtract the smallest loss
    for (size_t k=1; k<=max_action; k++)
      if (rollout[k-1].alive)
        loss_vector[k-1].x -= min_loss;
  }

  uint32_t uniform_exploration(state s0, float& prob_sampled_action)
  {
    //find how many valid actions
    size_t nb_allowed_actions = max_action;
    if( task.allowed ) {  
      for (uint32_t k=1; k<=max_action; k++) {
        if( !task.allowed(s0,k) ) {
          nb_allowed_actions--;
          if (is_ldf) {
            nb_allowed_actions = k-1;
            break;
          }
        }
      }
    }

    uint32_t action = (size_t)(frand48() * nb_allowed_actions) + 1;
    if( task.allowed && nb_allowed_actions < max_action && !is_ldf) {
      //need to adjust action to the corresponding valid action
      for (uint32_t k=1; k<=action; k++) {
        if( !task.allowed(s0,k) ) action++;
      }
    }
    prob_sampled_action = (float) (1.0/nb_allowed_actions);
    return action;
  }

  void get_contextual_bandit_loss_vector(vw&all, state s0)
  {
    float prob_sampled = 1.;
    uint32_t act = uniform_exploration(s0,prob_sampled);
    float loss = single_rollout(all,s0,act);

    loss_vector_cb.erase();
    for (uint32_t k=1; k<=max_action; k++) {
      if( task.allowed && !task.allowed(s0,k))
	break;
      
      CB::cb_class temp;
      temp.x = FLT_MAX;
      temp.weight_index = k;
      temp.prob_action = 0.;
      if( act == k ) {
        temp.x = loss;
        temp.prob_action = prob_sampled;
      }
      loss_vector_cb.push_back(temp);
    }
  }

  void generate_state_example(vw&all, state s0)
  {
    // start by doing rollouts so we can get costs
    loss_vector.erase();
    loss_vector_cb.erase();
    if( rollout_all_actions ) {
      parallel_rollout(all, s0);      
    }
    else {
      get_contextual_bandit_loss_vector(all, s0);
    }

    if (loss_vector.size() <= 1 && loss_vector_cb.size() == 0) {
      // nothing interesting to do!
      return;
    }

    // now, generate training examples
    if (!is_ldf) {
      total_examples_generated++;

      example* ec;
      task.cs_example(all, s0, ec, true);
      void* old_label = ec->ld;

      if(rollout_all_actions) {
        CSOAA::label ld = { loss_vector };
        ec->ld = (void*)&ld;
      } 
      else {
        CB::label ld = { loss_vector_cb };
        ec->ld = (void*)&ld;
      }
      SearnUtil::add_policy_offset(all, ec, increment, current_policy);
      base_learner(&all,ec);
      SearnUtil::remove_policy_offset(all, ec, increment, current_policy);
      ec->ld = old_label;
      task.cs_example(all, s0, ec, false);
    } else { // is_ldf
      //TODO: support ldf with contextual bandit base learner
      old_labels.erase();
      new_labels.erase();

      for (uint32_t k=1; k<=max_action; k++) {
        if (rollout[k-1].alive) {
          OAA::mc_label ld = { k, loss_vector[k-1].x };
          new_labels.push_back(ld);
        } else {
          OAA::mc_label ld = { k, 0. };
          new_labels.push_back(ld);
        }
      }

      //      cerr << "vvvvvvvvvvvvvvvvvvvvvvvvvvvv" << endl;

      for (uint32_t k=1; k<=max_action; k++) {
        if (!rollout[k-1].alive) break;

        total_examples_generated++;

        task.cs_ldf_example(all, s0, k, global_example_set[k-1], true);
        old_labels.push_back(global_example_set[k-1]->ld);
        global_example_set[k-1]->ld = (void*)(&new_labels[k-1]);
        SearnUtil::add_policy_offset(all, global_example_set[k-1], increment, current_policy);
        if (PRINT_DEBUG_INFO) { cerr << "add_policy_offset, max_action=" << max_action << ", total_number_of_policies=" << total_number_of_policies << ", current_policy=" << current_policy << endl;}
        base_learner(&all,global_example_set[k-1]);
      }

      //      cerr << "============================ (empty = " << empty_example << ")" << endl;
      empty_example->in_use = true;
      base_learner(&all,empty_example);

      for (uint32_t k=1; k<=max_action; k++) {
        if (!rollout[k-1].alive) break;
        SearnUtil::remove_policy_offset(all, global_example_set[k-1], increment, current_policy);
        global_example_set[k-1]->ld = old_labels[k-1];
        task.cs_ldf_example(all, s0, k, global_example_set[k-1], false);
      }
      //      cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
    }

  }

  void run_prediction(vw&all, state s0, bool allow_oracle, bool allow_current, bool track_actions, std::vector<action>* action_sequence)
  {
    int step = 1;
    while (!task.final(s0)) {
      uint32_t action = searn_predict(all, s0, step, allow_oracle, allow_current, NULL);
      if (track_actions)
        action_sequence->push_back(action);

      task.step(s0, action);
      step++;
    }
  }
  
/*
  struct beam_info_struct {
    vw&all;
    bool allow_oracle;
    bool allow_current;
  };

  void run_prediction_beam_iter(Beam::beam*b, size_t bucket_id, state s0, float cur_loss, void*args)
  {
    beam_info_struct* bi = (beam_info_struct*)args;

    if (task.final(s0)) return;

    v_array< pair<uint32_t,float> > partial_predictions;
    searn_predict(bi->all, s0, bucket_id, bi->allow_oracle, bi->allow_current, &partial_predictions);
    for (size_t i=0; i<partial_predictions.size(); i++) {
      state s1 = task.copy(s0);
      float new_loss = cur_loss + partial_predictions[i].second;
      uint32_t action = partial_predictions[i].first;
      task.step( s1, action );
      b->put( task.bucket(s1), s1, action, new_loss );
    }
  }

  void run_prediction_beam(vw&all, size_t max_beam_size, state s0, bool allow_oracle, bool allow_current, bool track_actions, std::vector<action>* action_sequence)
  {
    Beam::beam *b = new Beam::beam(task.equivalent, task.hash, max_beam_size);

    beam_info_struct bi = { all, allow_oracle, allow_current };

    b->put(task.bucket(s0), s0, 0, 0.);
    size_t current_bucket = 0;
    while (true) {
      current_bucket = b->get_next_bucket(current_bucket);
      if (current_bucket == 0) break;
      b->iterate(current_bucket, run_prediction_beam_iter, &bi);
    }

    if (track_actions && (action_sequence != NULL))
      b->get_best_output(action_sequence);

    delete b;
  }

  //  void hm_free_state_copies(state s, action a) { 
  //    task.finish(s);
  //  }

  */
  void do_actual_learning(vw&all)
  {
    // there are two cases:
    //   * is_singleline --> look only at ec_seq[0]
    //   * otherwise     --> look at everything

    if (ec_seq.size() == 0)
      return;

    // generate the start state
    state s0;
    if (is_singleline)
      task.start_state(ec_seq[0], &s0);
    else
      task.start_state_multiline(ec_seq.begin, ec_seq.size(), &s0);

    state s0copy = NULL;
    bool  is_test = task.is_test_example(ec_seq.begin, ec_seq.size());
    if (!is_test) {
      s0copy = task.copy(s0);
      all.sd->example_number++;
      all.sd->total_features    += searn_num_features;
      all.sd->weighted_examples += 1.;
    }
    bool will_print = is_test || should_print_update(all) || will_global_print_label(all);

    searn_num_features = 0;
    std::vector<action> action_sequence;

    // if we are using adaptive beta, update it to take into account the latest updates
    if( adaptive_beta ) beta = 1.f - powf(1.f - alpha,(float)total_examples_generated);
    
    run_prediction(all, s0, false, true, will_print, &action_sequence);
    global_print_label(all, ec_seq[0], s0, action_sequence);

    if (!is_test) {
      float loss = task.loss(s0);
      all.sd->sum_loss += loss;
      all.sd->sum_loss_since_last_dump += loss;
    }

    print_update(all, s0, action_sequence);
    
    task.finish(s0);

    if (is_test || !all.training)
      return;

    s0 = s0copy;

    // training examples only get here
    int step = 1;
    while (!task.final(s0)) {
      // if we are using adaptive beta, update it to take into account the latest updates
      if( adaptive_beta ) beta = 1.f - powf(1.f - alpha,(float)total_examples_generated);

      // first, make a prediction (we don't want to bias ourselves if
      // we're using the current policy to predict)
      uint32_t action = searn_predict(all, s0, step, true, allow_current_policy, NULL);

      // generate training example for the current state
      generate_state_example(all, s0);

      // take the prescribed step
      task.step(s0, action);

      step++;
    }
    task.finish(s0);
    if (do_recombination) {  // we need to free a bunch of memory
      //      past_states->iter(&hm_free_state_copies);
      free_unfreed_states();
      past_states->clear();
    }
  }

  void process_next_example(vw&all, example *ec)
  {
    bool is_real_example = true;

    if (is_singleline) {
      if (ec_seq.size() == 0)
        ec_seq.push_back(ec);
      else
        ec_seq[0] = ec;

      do_actual_learning(all);
    } else {  
      // is multiline
      if (ec_seq.size() >= all.p->ring_size - 2) { // give some wiggle room
        std::cerr << "warning: length of sequence at " << ec->example_counter << " exceeds ring size; breaking apart" << std::endl;
        do_actual_learning(all);
        clear_seq(all);
      }

      if (OAA::example_is_newline(ec)) {
        do_actual_learning(all);
        clear_seq(all);
        //CSOAA_LDF::global_print_newline(all);
	VW::finish_example(all, ec);
        is_real_example = false;
      } else {
        ec_seq.push_back(ec);
      }
    }

    // for both single and multiline
    if (is_real_example) {
      read_example_this_loop++;
      read_example_last_id = ec->example_counter;
      if (ec->pass != read_example_last_pass) {
        read_example_last_pass = ec->pass;
        passes_since_new_policy++;
        if (passes_since_new_policy >= passes_per_policy) {
          passes_since_new_policy = 0;
          if(all.training)
            current_policy++;
          if (current_policy > total_number_of_policies) {
            std::cerr << "internal error (bug): too many policies; not advancing" << std::endl;
            current_policy = total_number_of_policies;
          }
          //reset searn_trained_nb_policies in options_from_file so it is saved to regressor file later
          std::stringstream ss;
          ss << current_policy;
          VW::cmd_string_replace_value(all.options_from_file,"--searn_trained_nb_policies", ss.str());
        }
      }
    }
  }

  void drive(void*in)
  {
    vw*all = (vw*)in;
    // initialize everything
    
    const char * header_fmt = "%-10s %-10s %8s %15s %24s %22s %8s %5s %5s %15s %15s\n";

    fprintf(stderr, header_fmt, "average", "since", "sequence", "example",   "current label", "current predicted",  "current",  "cur", "cur", "predic.", "examples");
    fprintf(stderr, header_fmt, "loss",  "last",  "counter",  "weight", "sequence prefix",   "sequence prefix", "features", "pass", "pol",    "made",   "gener.");
    cerr.precision(5);

    initialize_memory();

    example* ec = NULL;
    read_example_this_loop = 0;
    while (true) {
      if ((ec = get_example(all->p)) != NULL) { // semiblocking operation
        process_next_example(*all, ec);
      } else if (parser_done(all->p)) {
        if (!is_singleline)
          do_actual_learning(*all);
        break;
      }
    }

    if( all->training ) {
      std::stringstream ss1;
      std::stringstream ss2;
      ss1 << (current_policy+1);
      //use cmd_string_replace_value in case we already loaded a predictor which had a value stored for --searn_trained_nb_policies
      VW::cmd_string_replace_value(all->options_from_file,"--searn_trained_nb_policies", ss1.str()); 
      ss2 << total_number_of_policies;
      //use cmd_string_replace_value in case we already loaded a predictor which had a value stored for --searn_total_nb_policies
      VW::cmd_string_replace_value(all->options_from_file,"--searn_total_nb_policies", ss2.str());
    }
  }


}


namespace ImperativeSearn {
  const char INIT_TEST  = 0;
  const char INIT_TRAIN = 1;
  const char LEARN      = 2;

  inline bool isLDF(searn_struct* srn) { return (srn->A == 0); }

  size_t choose_policy(searn_struct* srn, bool allow_optimal)
  {
    uint32_t seed = 0; // TODO: srn->read_example_last_id * 2147483 + srn->t * 2147483647;
    return SearnUtil::random_policy(seed, srn->beta, srn->allow_current_policy, srn->current_policy, allow_optimal, srn->rollout_all_actions);
  }

  v_array<CSOAA::wclass> get_all_labels(searn_struct* srn, size_t num_ec, v_array<uint32_t> *yallowed)
  {
    if (isLDF(srn)) {
      v_array<CSOAA::wclass> ret;  // TODO: cache these!
      for (uint32_t i=0; i<num_ec; i++) {
        CSOAA::wclass cost = { FLT_MAX, i, 1., 0. };
        ret.push_back(cost);
      }
      return ret;
    }
    // is not LDF
    if (yallowed == NULL) {
      v_array<CSOAA::wclass> ret;  // TODO: cache this!
      for (uint32_t i=1; i<=srn->A; i++) {
        CSOAA::wclass cost = { FLT_MAX, i, 1., 0. };
        ret.push_back(cost);
      }
      return ret;
    }
    v_array<CSOAA::wclass> ret;
    for (size_t i=0; i<yallowed->size(); i++) {
      CSOAA::wclass cost = { FLT_MAX, (*yallowed)[i], 1., 0. };
      ret.push_back(cost);
    }
    return ret;
  }

  uint32_t single_prediction_LDF(vw& all, example** ecs, size_t num_ec, size_t pol)
  {
    assert(pol > 0);
    // TODO
    return 0;
  }

  uint32_t single_prediction_notLDF(vw& all, example* ec, v_array<CSOAA::wclass> valid_labels, size_t pol)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;
    assert(pol > 0);

    void* old_label = ec->ld;
    ec->ld = (void*)&valid_labels;
    SearnUtil::add_policy_offset(all, ec, srn->increment, pol);

    srn->base_learner(&all, ec);
    srn->total_predictions_made++;
    srn->num_features += ec->num_features;
    uint32_t final_prediction = (uint32_t)(*(OAA::prediction_t*)&(ec->final_prediction));

    SearnUtil::remove_policy_offset(all, ec, srn->increment, pol);
    ec->ld = old_label;

    return final_prediction;
  }

  template<class T> T choose_random(v_array<T> opts) {
    float r = frand48();
    assert(opts.size() > 0);
    return opts[((float)opts.size()) * r];
  }

  uint32_t single_action(vw& all, example** ecs, size_t num_ec, v_array<CSOAA::wclass> valid_labels, size_t pol, v_array<uint32_t> *ystar) {
    cerr << "pol=" << pol << " ystar.size()=" << ystar->size() << " ystar[0]=" << ((ystar->size() > 0) ? (*ystar)[0] : 0) << endl;
    if (pol == 0) { // optimal policy
      if ((ystar == NULL) || (ystar->size() == 0))
        return choose_random<CSOAA::wclass>(valid_labels).weight_index;
      else
        return choose_random<uint32_t>(*ystar);
    } else {        // learned policy
      if (isLDF((searn_struct*)all.searnstr))
        return single_prediction_LDF(all, ecs, num_ec, pol);
      else
        return single_prediction_notLDF(all, *ecs, valid_labels, pol);
    }
  }

  void clear_snapshot(vw& all)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;
    for (size_t i=0; i<srn->snapshot_data.size(); i++) {
      v_array< pair<void*,size_t> > data = srn->snapshot_data[i].second;
      for (size_t j=0; j<data.size(); j++)
        free(data[j].first);
      data.erase();
    }
    srn->snapshot_data.erase();
  }

  // if not LDF:
  //   *ecs should be a pointer to THE example
  //   num_ec == 0
  //   yallowed:
  //     == NULL means ANY action is okay [1..M]
  //     != NULL means only the given actions are okay
  // if LDF:
  //   *ecs .. *(ecs+num_ec-1) should be valid actions
  //   num_ec > 0
  //   yallowed MUST be NULL (why would you give me an impossible action?)
  // in both cases:
  //   ec->ld is ignored
  //   ystar:
  //     == NULL (or empty) means we don't know the oracle label
  //     otherwise          means the oracle could do any of the listed actions
  uint32_t searn_predict(vw& all, example** ecs, size_t num_ec, v_array<uint32_t> *yallowed, v_array<uint32_t> *ystar)  // num_ec == 0 means normal example, >0 means ldf, yallowed==NULL means all allowed, ystar==NULL means don't know
  {
    searn_struct *srn = (searn_struct*)all.searnstr;

    // check ldf sanity
    if (!isLDF(srn)) {
      assert(num_ec == 0); // searntask is trying to define an ldf example in a non-ldf problem
    } else { // is LDF
      assert(num_ec != 0); // searntask is trying to define a non-ldf example in an ldf problem" << endl;
      assert(yallowed == NULL); // searntask is trying to specify allowed actions in an ldf problem" << endl;
    }

    if (srn->state == INIT_TEST) {
      size_t pol = choose_policy(srn, false);
      v_array<CSOAA::wclass> valid_labels = get_all_labels(srn, num_ec, yallowed);
      uint32_t a = single_action(all, ecs, num_ec, valid_labels, pol, ystar);
      srn->t++;
      valid_labels.erase(); valid_labels.delete_v();
      return a;
    }
    if (srn->state == INIT_TRAIN) {
      size_t pol = choose_policy(srn, true);
      v_array<CSOAA::wclass> valid_labels = get_all_labels(srn, num_ec, yallowed);
      uint32_t a = single_action(all, ecs, num_ec, valid_labels, pol, ystar);
      srn->train_action.push_back(a);
      srn->train_labels.push_back(valid_labels);
      srn->t++;
      return a;
    }
    if (srn->state == LEARN) {
      if (srn->t < srn->learn_t) {
        assert(srn->t < srn->train_action.size());
        srn->t++;
        return srn->train_action[srn->t-1];
      } else if (srn->t == srn->learn_t) {
        if (srn->learn_example_copy == NULL) {
          size_t num_to_copy = (num_ec == 0) ? 1 : num_ec;
          srn->learn_example_len = num_to_copy;
          srn->learn_example_copy = (example**)SearnUtil::calloc_or_die(num_to_copy, sizeof(example*));
          for (size_t n=0; n<num_to_copy; n++) {
            srn->learn_example_copy[n] = alloc_example(sizeof(CSOAA::label));
            VW::copy_example_data(srn->learn_example_copy[n], ecs[n], sizeof(CSOAA::label), all.p->lp->copy_label);
          }
          cerr << "copying example to " << srn->learn_example_copy << endl;
        }
        srn->t++;
        return srn->learn_a;
      } else {
        size_t pol = choose_policy(srn, true);
        v_array<CSOAA::wclass> valid_labels = get_all_labels(srn, num_ec, yallowed);
        uint32_t a = single_action(all, ecs, num_ec, valid_labels, pol, ystar);
        srn->t++;
        valid_labels.erase(); valid_labels.delete_v();
        return a;
      }
      assert(false);
    }
    cerr << "fail: searn got into ill-defined state (" << (int)srn->state << ")" << endl;
    exit(-1);
  }

  void searn_declare_loss(vw& all, size_t predictions_since_last, float incr_loss)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;

    if (srn->t != srn->loss_last_step + predictions_since_last) {
      cerr << "fail: searntask hasn't counted its predictions correctly.  current time step=" << srn->t << ", last declaration at " << srn->loss_last_step << ", declared # of predictions since then is " << predictions_since_last << endl;
      exit(-1);
    }
    srn->loss_last_step = srn->t;
    if (srn->state == INIT_TEST)
      srn->test_loss += incr_loss;
    else if (srn->state == INIT_TRAIN)
      srn->train_loss += incr_loss;
    else
      srn->learn_loss += incr_loss;
  }

  void searn_snapshot(vw& all, size_t index, size_t tag, void* data, size_t sizeof_data)
  {
    //searn_struct *srn = (searn_struct*)all.searnstr;
    return;
  }

  v_array<size_t> get_training_timesteps(vw& all)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;
    v_array<size_t> timesteps;
    for (size_t t=0; t<srn->T; t++)
      timesteps.push_back(t);
    return timesteps;
  }

  void generate_training_example(vw& all, example** ec, size_t len, v_array<CSOAA::wclass> labels, v_array<float> losses)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;

    assert(labels.size() == losses.size());
    for (size_t i=0; i<labels.size(); i++)
      labels[i].x = losses[i];

    if (!isLDF(srn)) {
      void* old_label = ec[0]->ld;
      CSOAA::label new_label = { labels };
      ec[0]->ld = (void*)&new_label;
      SearnUtil::add_policy_offset(all, ec[0], srn->increment, srn->current_policy);
      srn->base_learner(&all, ec[0]);
      SearnUtil::remove_policy_offset(all, ec[0], srn->increment, srn->current_policy);
      ec[0]->ld = old_label;
      srn->total_examples_generated++;
    } else { // isLDF
      //TODO
    }
  }

  void train_single_example(vw& all, example**ec, size_t len)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;

    // do an initial test pass to compute output (and loss)
    cerr << "======================================== INIT TEST ========================================" << endl;
    srn->state = INIT_TEST;
    srn->t = 0;
    srn->T = 0;
    srn->loss_last_step = 0;
    srn->test_loss = 0.f;
    srn->train_loss = 0.f;
    srn->learn_loss = 0.f;
    srn->learn_example_copy = NULL;
    srn->learn_example_len  = 0;
    srn->train_action.erase();
 
    srn->task.structured_predict(all, ec, len);

    if (srn->t == 0)
      return;  // there was no data!

    // do a pass over the data allowing oracle and snapshotting
    cerr << "======================================== INIT TRAIN ========================================" << endl;
    srn->state = INIT_TRAIN;
    srn->t = 0;
    srn->loss_last_step = 0;
    clear_snapshot(all);

    srn->task.structured_predict(all, ec, len);

    if (srn->t == 0) {
      clear_snapshot(all);
      return;  // there was no data
    }

    srn->T = srn->t;

    // generate training examples on which to learn
    cerr << "======================================== LEARN ========================================" << endl;
    srn->state = LEARN;
    v_array<size_t> tset = get_training_timesteps(all);
    for (size_t t=0; t<tset.size(); t++) {
      v_array<CSOAA::wclass> aset = srn->train_labels[t];
      srn->learn_t = t;
      srn->learn_losses.erase();

      for (size_t i=0; i<aset.size(); i++) {
        if (aset[i].weight_index == srn->train_action[srn->learn_t])
          srn->learn_losses.push_back( srn->train_loss );
        else {
          srn->t = 0;
          srn->learn_a = aset[i].weight_index;
          srn->loss_last_step = 0;
          srn->learn_loss = 0.f;

          cerr << "learn_t = " << srn->learn_t << " || learn_a = " << srn->learn_a << endl;
          srn->task.structured_predict(all, ec, len);

          srn->learn_losses.push_back( srn->learn_loss );
          cerr << "total loss: " << srn->learn_loss << endl;
        }
      }

      if (srn->learn_example_copy != NULL) {
        generate_training_example(all, srn->learn_example_copy, srn->learn_example_len, aset, srn->learn_losses);

        for (size_t n=0; n<srn->learn_example_len; n++) {
          dealloc_example(CSOAA::delete_label, *srn->learn_example_copy[n]);
          free(srn->learn_example_copy[n]);
        }
        free(srn->learn_example_copy);
        srn->learn_example_copy = NULL;
        srn->learn_example_len  = 0;
      } else {
        cerr << "warning: searn did not generate an example for a given time-step" << endl;
      }
    }
    tset.erase(); tset.delete_v();

    clear_snapshot(all);
    srn->train_action.erase();
    srn->train_action.delete_v();
    for (size_t i=0; i<srn->train_labels.size(); i++) {
      srn->train_labels[i].erase();
      srn->train_labels[i].delete_v();
    }
    srn->train_labels.erase();
    srn->train_labels.delete_v();
  }


  void clear_seq(vw&all)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;
    if (srn->ec_seq.size() > 0) 
      for (example** ecc=srn->ec_seq.begin; ecc!=srn->ec_seq.end; ecc++) {
	VW::finish_example(all, *ecc);
      }
    srn->ec_seq.erase();
  }

  void do_actual_learning(vw&all)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;
    if (srn->ec_seq.size() == 0)
      return;  // nothing to do :)

    train_single_example(all, srn->ec_seq.begin, srn->ec_seq.size());
  }

  void searn_learn(void*in, example*ec) {
    vw all = *(vw*)in;
    searn_struct *srn = (searn_struct*)all.searnstr;

    if (srn->ec_seq.size() >= all.p->ring_size - 2) { // give some wiggle room
      std::cerr << "warning: length of sequence at " << ec->example_counter << " exceeds ring size; breaking apart" << std::endl;
      do_actual_learning(all);
      clear_seq(all);
    }
    
    bool is_real_example = true;

    if (OAA::example_is_newline(ec)) {
      do_actual_learning(all);
      clear_seq(all);
      VW::finish_example(all, ec);
      is_real_example = false;
    } else {
      srn->ec_seq.push_back(ec);
    }
        
    if (is_real_example) {
      srn->read_example_this_loop++;
      srn->read_example_last_id = ec->example_counter;
      if (ec->pass != srn->read_example_last_pass) {
        srn->read_example_last_pass = ec->pass;
        srn->passes_since_new_policy++;
        if (srn->passes_since_new_policy >= srn->passes_per_policy) {
          srn->passes_since_new_policy = 0;
          if(all.training)
            srn->current_policy++;
          if (srn->current_policy > srn->total_number_of_policies) {
            std::cerr << "internal error (bug): too many policies; not advancing" << std::endl;
            srn->current_policy = srn->total_number_of_policies;
          }
          //reset searn_trained_nb_policies in options_from_file so it is saved to regressor file later
          std::stringstream ss;
          ss << srn->current_policy;
          VW::cmd_string_replace_value(all.options_from_file,"--searn_trained_nb_policies", ss.str());
        }
      }
    }
  }

  void searn_drive(void*in) {
    vw all = *(vw*)in;
    searn_struct *srn = (searn_struct*)all.searnstr;

    const char * header_fmt = "%-10s %-10s %8s %15s %24s %22s %8s %5s %5s %15s %15s\n";
    
    fprintf(stderr, header_fmt, "average", "since", "sequence", "example",   "current label", "current predicted",  "current",  "cur", "cur", "predic.", "examples");
    fprintf(stderr, header_fmt, "loss",  "last",  "counter",  "weight", "sequence prefix",   "sequence prefix", "features", "pass", "pol",    "made",   "gener.");
    cerr.precision(5);

    example* ec = NULL;
    srn->read_example_this_loop = 0;
    while (true) {
      if ((ec = get_example(all.p)) != NULL) { // semiblocking operation
        searn_learn(in, ec);
      } else if (parser_done(all.p)) {
        do_actual_learning(all);
        break;
      }
    }

    if( all.training ) {
      std::stringstream ss1;
      std::stringstream ss2;
      ss1 << (srn->current_policy+1);
      //use cmd_string_replace_value in case we already loaded a predictor which had a value stored for --searnimp_trained_nb_policies
      VW::cmd_string_replace_value(all.options_from_file,"--searnimp_trained_nb_policies", ss1.str()); 
      ss2 << srn->total_number_of_policies;
      //use cmd_string_replace_value in case we already loaded a predictor which had a value stored for --searnimp_total_nb_policies
      VW::cmd_string_replace_value(all.options_from_file,"--searnimp_total_nb_policies", ss2.str());
    }
  }


  void searn_initialize(vw& all)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;

    srn->predict = searn_predict;
    srn->declare_loss = searn_declare_loss;
    srn->snapshot = searn_snapshot;

    srn->A = 3; // TODO
    srn->beta = 0.5;  // TODO
    srn->allow_current_policy = false;  // TODO
    srn->rollout_all_actions = true; // TODO
    srn->increment = 99999; // TODO
    srn->base_learner = NULL; // TODO
    srn->num_features = 0;
    srn->current_policy = 1;
    srn->state = 0;

    srn->passes_per_policy = 100;     //this should be set to the same value as --passes for dagger

    srn->read_example_this_loop = 0;
    srn->read_example_last_id = 0;
    srn->passes_since_new_policy = 0;
    srn->read_example_last_pass = 0;
    srn->total_examples_generated = 0;
    srn->total_predictions_made = 0;
  }

  void searn_finish(void*in)
  {
    vw*all = (vw*)in;
    searn_struct *srn = (searn_struct*)all->searnstr;

    cerr << "searn_finish" << endl;

    clear_seq(*all);
    srn->ec_seq.delete_v();

    clear_snapshot(*all);

    for (size_t i=0; i<srn->train_labels.size(); i++) {
      srn->train_labels[i].erase();
      srn->train_labels[i].delete_v();
    }
    srn->train_labels.erase(); srn->train_labels.delete_v();
    srn->train_action.erase(); srn->train_action.delete_v();

    srn->learn_losses.erase(); srn->learn_losses.delete_v();

    if (srn->task.finish != NULL)
      srn->task.finish(*all);
    if (srn->task.finish != NULL)
      srn->base_finish(all);
  }

  void parse_flags(vw&all, std::vector<std::string>&opts, po::variables_map& vm, po::variables_map& vm_file)
  {
    searn_struct *srn = (searn_struct*)all.searnstr;

    searn_initialize(all);

    searn_task mytask = { SequenceTask_Easy::initialize, 
                          SequenceTask_Easy::finish, 
                          SequenceTask_Easy::structured_predict_v1
                        };

    srn->task = mytask;
    
    all.driver = searn_drive;
    srn->base_learner = all.learn;
    all.learn = searn_learn;
    srn->base_finish = all.finish;
    all.finish = searn_finish;
  }

}
