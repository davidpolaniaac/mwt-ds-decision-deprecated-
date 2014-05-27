#include "gd.h"
#include "rand48.h"
#include "simple_label.h"
#include "allreduce.h"
#include "accumulate.h"
#include <float.h>

//#undef NDEBUG
//#define DEBUG
#include <cassert>

#define PARALLEL_ENABLE

using namespace std;
using namespace LEARNER;

namespace StagewisePoly
{
  static const uint32_t parent_bit = 1;
  static const uint32_t cycle_bit = 2;
  static const uint32_t tree_atomics = 134;
  static const float tolerance = 1e-9;
  static const int mult_const = 95104348457;
  static const uint32_t hash_mod_table[] = {
    [0] = 0,
    [1] = 1,
    [2] = 3,
    [3] = 3,
    [4] = 11,
    [5] = 11,
    [6] = 43,
    [7] = 43,
    [8] = 171,
    [9] = 171,
    [10] = 683,
    [11] = 683,
    [12] = 2731,
    [13] = 2731,
    [14] = 10923,
    [15] = 10923,
    [16] = 43691,
    [17] = 43691,
    [18] = 174763,
    [19] = 174763,
    [20] = 699051,
    [21] = 699051,
    [22] = 2796203,
    [23] = 2796203,
    [24] = 11184811,
    [25] = 11184811,
    [26] = 44739243,
    [27] = 44739243,
    [28] = 178956971,
    [29] = 178956971,
    [30] = 715827883,
    [31] = 715827883,
    [32] = 2863311531
  };

  struct sort_data {
    float wval;
    uint32_t wid;
  };

  struct stagewise_poly
  {
    vw *all;

    float sched_exponent;
    float magic_argument;
    uint32_t batch_sz;

    sort_data *sd;
    uint32_t sd_len;
    uint8_t *depthsbits; //interleaved array storing depth information and parent/cycle bits

    uint64_t sum_sparsity; //of synthetic example
    uint64_t sum_input_sparsity; //of input example
    uint64_t num_examples;
    //following three are for parallel (see end_pass())
    uint64_t sum_sparsity_sync;
    uint64_t sum_input_sparsity_sync;
    uint64_t num_examples_sync;

    example synth_ec;
    //following is bookkeeping in synth_ec creation (dfs)
    feature synth_rec_f;
    example *original_ec;
    uint32_t cur_depth;
    bool training;
    size_t numpasses;

#ifdef DEBUG
    uint32_t max_depth;
    uint32_t depths[100000];
#endif //DEBUG
  };


  inline uint32_t stride_shift(const stagewise_poly &poly, uint32_t idx)
  {
    return idx << poly.all->reg.stride_shift;
  }

  inline uint32_t stride_un_shift(const stagewise_poly &poly, uint32_t idx)
  {
    return idx >> poly.all->reg.stride_shift;
  }

  inline uint32_t wid_mask(const stagewise_poly &poly, uint32_t wid)
  {
    return wid & poly.all->reg.weight_mask;
  }

  inline uint32_t wid_mask_un_shifted(const stagewise_poly &poly, uint32_t wid)
  {
    return stride_un_shift(poly, wid & poly.all->reg.weight_mask);
  }

  inline uint32_t constant_feat(const stagewise_poly &poly)
  {
    return stride_shift(poly, constant);
  }

  inline uint32_t constant_feat_masked(const stagewise_poly &poly)
  {
    return wid_mask(poly, constant_feat(poly));
  }


  inline uint32_t depthsbits_sizeof(const stagewise_poly &poly)
  {
    return 2 * poly.all->length() * sizeof(uint8_t);
  }

  void depthsbits_create(stagewise_poly &poly)
  {
    poly.depthsbits = (uint8_t *) malloc(depthsbits_sizeof(poly));
    for (uint32_t i = 0; i < poly.all->length() * 2; i += 2) {
      poly.depthsbits[i] = 0xff;
      poly.depthsbits[i+1] = 0;
    }
  }

  void depthsbits_destroy(stagewise_poly &poly)
  {
    free(poly.depthsbits);
  }

  inline bool parent_get(const stagewise_poly &poly, uint32_t wid)
  {
    assert(wid % stride_shift(poly, 1) == 0);
    return poly.depthsbits[wid_mask_un_shifted(poly, wid) * 2 + 1] & parent_bit;
  }

  inline void parent_toggle(stagewise_poly &poly, uint32_t wid)
  {
    assert(wid % stride_shift(poly, 1) == 0);
    poly.depthsbits[wid_mask_un_shifted(poly, wid) * 2 + 1] ^= parent_bit;
  }

  inline bool cycle_get(const stagewise_poly &poly, uint32_t wid)
  {
    assert(wid % stride_shift(poly, 1) == 0);
    return poly.depthsbits[wid_mask_un_shifted(poly, wid) * 2 + 1] & cycle_bit;
  }

  inline void cycle_toggle(stagewise_poly &poly, uint32_t wid)
  {
    assert(wid % stride_shift(poly, 1) == 0);
    poly.depthsbits[wid_mask_un_shifted(poly, wid) * 2 + 1] ^= cycle_bit;
  }

  inline uint8_t min_depths_get(const stagewise_poly &poly, uint32_t wid)
  {
    assert(wid % stride_shift(poly, 1) == 0);
    return poly.depthsbits[stride_un_shift(poly, wid) * 2];
  }

  inline void min_depths_set(stagewise_poly &poly, uint32_t wid, uint8_t depth)
  {
    assert(wid % stride_shift(poly, 1) == 0);
    poly.depthsbits[stride_un_shift(poly, wid) * 2] = depth;
  }

  //Note.  OUTPUT & INPUT masked.
  //It is very important that this function is invariant to stride.
  inline uint32_t child_wid(const stagewise_poly &poly, uint32_t wi_atomic, uint32_t wi_general)
  {
    assert(wi_atomic == wid_mask(poly, wi_atomic));
    assert(wi_general == wid_mask(poly, wi_general));
    assert((wi_atomic & (stride_shift(poly, 1) - 1)) == 0);
    assert((wi_general & (stride_shift(poly, 1) - 1)) == 0);

    if (poly.magic_argument == 0) { //XXX TEMPORARY CONDITIONAL BLOCK DON'T HATE ME
      if (wi_atomic == constant_feat_masked(poly))
        return wi_general;
      else if (wi_general == constant_feat_masked(poly))
        return wi_atomic;
      else if (wi_atomic == wi_general) {
        uint64_t wi_2_64 = stride_un_shift(poly, wi_general);
        return wid_mask(poly, stride_shift(poly, (size_t)(merand48(wi_2_64) * ((poly.all->length()) - 1))));
      } else
        return wid_mask(poly, stride_shift(poly, ((stride_un_shift(poly, wi_atomic) * mult_const) ^ (stride_un_shift(poly, wi_general) * mult_const))));
    } else if (poly.magic_argument == 1) {
      if (wi_atomic == constant_feat_masked(poly))
        return wi_general;
      else if (wi_general == constant_feat_masked(poly))
        return wi_atomic;
      else {
        //This is basically the "Fowler–Noll–Vo" hash.  Ideally, the hash would be invariant
        //to the monomial, whereas this here is sensitive to the path followed, but whatever.
        return wid_mask(poly, stride_shift(poly, stride_un_shift(poly, wi_atomic)
              ^ (16777619 * stride_un_shift(poly, wi_general))));
      }
    } else if (poly.magic_argument == 2) {
      return 0; //was going to tune constants and add modulus to FNV above but became lazy.
    } else if (poly.magic_argument == 3) {
      if (wi_atomic == constant_feat_masked(poly))
        return wi_general;
      else if (wi_general == constant_feat_masked(poly))
        return wi_atomic;
      else if (wi_atomic == wi_general) {
        uint64_t wi_2_64 = stride_un_shift(poly, wi_general);
        return wid_mask(poly, stride_shift(poly, (size_t)(merand48(wi_2_64) * ((poly.all->length()) - 1))));
      } else {
        assert(3 * hash_mod_table[poly.all->num_bits] % poly.all->length() == 1);
        uint32_t xa = 3 * stride_un_shift(poly, wi_atomic) % poly.all->length();
        uint32_t xg = 3 * stride_un_shift(poly, wi_general) % poly.all->length();
        return wid_mask(poly, stride_shift(poly, (xa ^ xg) * hash_mod_table[poly.all->num_bits]));
      }
    } else {
      return 0; //deal with it.
    }
  }

  void sort_data_create(stagewise_poly &poly)
  {
    poly.sd = NULL;
    poly.sd_len = 0;
  }

  void sort_data_ensure_sz(stagewise_poly &poly, uint32_t len)
  {
    if (poly.sd_len < len) {
      uint32_t len_candidate = 2 * len;
#ifdef DEBUG
      cout << "resizing sort buffer; current size " << poly.sd_len;
#endif //DEBUG
      poly.sd_len = (len_candidate > poly.all->length()) ? poly.all->length() : len_candidate;
#ifdef DEBUG
      cout << ", new size " << poly.sd_len << endl;
#endif //DEBUG
      free(poly.sd); //okay for null.
      poly.sd = (sort_data *) malloc(poly.sd_len * sizeof(sort_data));
    }
    assert(len <= poly.sd_len);
  }

  void sort_data_destroy(stagewise_poly &poly)
  {
    free(poly.sd);
  }

#ifdef DEBUG
  int sort_data_compar(const void *a_v, const void *b_v)
  {
    return 2 * ( ((sort_data *) a_v)->wval < ((sort_data *) b_v)->wval ) - 1;
  }
#endif //DEBUG

  int sort_data_compar_heap(sort_data &a_v, sort_data &b_v)
  {
    return (a_v.wval > b_v.wval);
  }

  /*
   * Performance note.
   *
   * On my laptop (Intel(R) Core(TM) i7-3520M CPU @ 2.90GHz), with compiler
   * optimizations enabled, this routine takes ~0.001 seconds with -b 18 and
   * ~0.06 seconds with -b 24.  Since it is intended to run ~8 times in 1-pass
   * mode and otherwise once per pass, it is considered adequate.
   *
   * Another choice (implemented in another version) is to never process the
   * whole weight vector (e.g., by updating a hash set of nonzero weights after
   * creating the synthetic example, and only processing that here).  This
   * choice was implemented in a similar algorithm and performed well.
   */
  void sort_data_update_support(stagewise_poly &poly)
  {
    assert(poly.num_examples);
    uint32_t num_new_features = pow(poly.sum_input_sparsity * 1.0 / poly.num_examples, poly.sched_exponent);
    num_new_features = (num_new_features > poly.all->length()) ? poly.all->length() : num_new_features;
    sort_data_ensure_sz(poly, num_new_features);

    cout<<"Adding "<<num_new_features<<endl;

    sort_data *heap_end = poly.sd;
    make_heap(poly.sd, heap_end, sort_data_compar_heap); //redundant
    for (uint32_t i = 0; i != poly.all->length(); ++i) {
      uint32_t wid = stride_shift(poly, i);
      if (!parent_get(poly, wid) && wid != constant_feat_masked(poly)) {
        float wval = (fabsf(poly.all->reg.weight_vector[wid])
            * poly.all->reg.weight_vector[poly.all->normalized_idx + (wid)])
          /*
           * here's some depth penalization code.  It was found to not improve
           * statistical performance, and meanwhile it is verified as giving
           * a nontrivial computational hit, thus commented out.
           *
           * - poly.magic_argument
           * sqrtf(min_depths_get(poly, stride_shift(poly, i)) * 1.0 / poly.num_examples)
           */
          ;
        if (wval > tolerance) {
	  //cout<<wval<<" ";
          assert(heap_end >= poly.sd);
          assert(heap_end <= poly.sd + num_new_features);

          if (heap_end - poly.sd == num_new_features && poly.sd->wval < wval) {
            pop_heap(poly.sd, heap_end, sort_data_compar_heap);
            --heap_end;
          }

          assert(heap_end >= poly.sd);
          assert(heap_end < poly.sd + poly.sd_len);

          if (heap_end - poly.sd < num_new_features) {
            heap_end->wval = wval;
            heap_end->wid = wid;
            ++heap_end;
            push_heap(poly.sd, heap_end, sort_data_compar_heap);
          }
        }
      }
    }
    //cout<<endl;
    num_new_features = (uint32_t) (heap_end - poly.sd);

    cout<<"Added "<<num_new_features<<endl;

#ifdef DEBUG
    //eyeballing weights a pain if unsorted.
    qsort(poly.sd, num_new_features, sizeof(sort_data), sort_data_compar);
#endif //DEBUG

    for (uint32_t pos = 0; pos < num_new_features && pos < poly.sd_len; ++pos) {
      assert(!parent_get(poly, poly.sd[pos].wid)
          && poly.sd[pos].wval > tolerance
          && poly.sd[pos].wid != constant_feat_masked(poly));
      parent_toggle(poly, poly.sd[pos].wid);
#ifdef DEBUG
      cout
        << "Adding feature " << pos << "/" << num_new_features
        << " || wid " << poly.sd[pos].wid
        << " || sort value " << poly.sd[pos].wval
        << endl;
#endif //DEBUG
    }

#ifdef DEBUG
    cout << "depths:";
    for (uint32_t depth = 0; depth <= poly.max_depth && depth < sizeof(poly.depths) / sizeof(*poly.depths); ++depth)
      cout << "  [" << depth << "] = " << poly.depths[depth];
    cout << endl;
#endif //DEBUG
  }

  void synthetic_reset(stagewise_poly &poly, example &ec)
  {
    poly.synth_ec.ld = ec.ld;
    poly.synth_ec.tag = ec.tag;
    poly.synth_ec.example_counter = ec.example_counter;
    poly.synth_ec.ft_offset = ec.ft_offset;

    poly.synth_ec.test_only = ec.test_only;
    poly.synth_ec.end_pass = ec.end_pass;
    poly.synth_ec.sorted = ec.sorted;
    poly.synth_ec.in_use = ec.in_use;

    poly.synth_ec.atomics[tree_atomics].erase();
    poly.synth_ec.audit_features[tree_atomics].erase();
    poly.synth_ec.num_features = 0;
    poly.synth_ec.total_sum_feat_sq = 0;
    poly.synth_ec.sum_feat_sq[tree_atomics] = 0;
    poly.synth_ec.example_t = ec.example_t;

    if (poly.synth_ec.indices.size()==0)
      poly.synth_ec.indices.push_back(tree_atomics);
  }

  void synthetic_decycle(stagewise_poly &poly)
  {
    for (feature *f = poly.synth_ec.atomics[tree_atomics].begin;
        f != poly.synth_ec.atomics[tree_atomics].end; ++f) {
      assert(cycle_get(poly, f->weight_index));
      cycle_toggle(poly, f->weight_index);
    }
  }

  void synthetic_create_rec(stagewise_poly &poly, float v, float &w)
  {
    uint32_t wid_atomic = (uint32_t)((&w - poly.all->reg.weight_vector));
    uint32_t wid_cur = child_wid(poly, wid_atomic, poly.synth_rec_f.weight_index);
    assert(wid_atomic % stride_shift(poly, 1) == 0);

    //Note: only mutate learner state when in training mode.  This is because
    //the average test errors across multiple data sets should be equal to
    //the test error on the merged dataset (which is violated if the code
    //below is run at training time).
    if (poly.cur_depth < min_depths_get(poly, wid_cur) && poly.training) {
      if (parent_get(poly, wid_cur)) {
	//#ifdef DEBUG
        cout
          << "FOUND A TRANSPLANT!!! moving [" << wid_cur
          << "] from depth " << (uint32_t) min_depths_get(poly, wid_cur)
          << " to depth " << poly.cur_depth << endl;
	//#endif //DEBUG
        parent_toggle(poly, wid_cur);
      }
      min_depths_set(poly, wid_cur, poly.cur_depth);
    }

    if ( ! cycle_get(poly, wid_cur)
        && ((poly.cur_depth > 0xff ? 0xff : poly.cur_depth) == min_depths_get(poly, wid_cur))
       ) {
      cycle_toggle(poly, wid_cur);

#ifdef DEBUG
      ++poly.depths[poly.cur_depth];
#endif //DEBUG

      feature new_f = { v * poly.synth_rec_f.x, wid_cur };
      poly.synth_ec.atomics[tree_atomics].push_back(new_f);
      poly.synth_ec.num_features++;
      poly.synth_ec.sum_feat_sq[tree_atomics] += new_f.x * new_f.x;

      if (parent_get(poly, new_f.weight_index)) {
        feature parent_f = poly.synth_rec_f;
        poly.synth_rec_f = new_f;
        ++poly.cur_depth;
#ifdef DEBUG
        poly.max_depth = (poly.max_depth > poly.cur_depth) ? poly.max_depth : poly.cur_depth;
#endif //DEBUG
        GD::foreach_feature<stagewise_poly, synthetic_create_rec>(*(poly.all), *(poly.original_ec), poly);
        --poly.cur_depth;
        poly.synth_rec_f = parent_f;
      }
    }
    // else {
    //   cout<<"Skipping feature "<<cycle_get(poly, wid_cur)<<" "<<(uint32_t)min_depths_get(poly, wid_cur)<<" "<<poly.cur_depth<<" "<<wid_atomic<<" "<<poly.synth_rec_f.weight_index<<" "<<wid_cur<<endl;
    // }
  }

  void synthetic_create(stagewise_poly &poly, example &ec, bool training)
  {
    synthetic_reset(poly, ec);

    poly.cur_depth = 0;

    poly.synth_rec_f.x = 1.0;
    poly.synth_rec_f.weight_index = constant_feat_masked(poly);
    poly.original_ec = &ec;
    poly.training = training;
    /*
     * Another choice is to mark the constant feature as the single initial
     * parent, and recurse just on that feature (which arguably correctly interprets poly.cur_depth).
     * Problem with this is if there is a collision with the root...
     */
    //cout<<"Starting feature creation\n";
    GD::foreach_feature<stagewise_poly, synthetic_create_rec>(*poly.all, *poly.original_ec, poly);
    //cout<<"Finished feature creation\n";
    synthetic_decycle(poly);
    poly.synth_ec.total_sum_feat_sq = poly.synth_ec.sum_feat_sq[tree_atomics];

    if (training) {
      poly.sum_sparsity += poly.synth_ec.num_features;
      poly.sum_input_sparsity += ec.num_features;
      poly.num_examples += 1;
    }
  }

  void predict(stagewise_poly &poly, learner &base, example &ec)
  {
    synthetic_create(poly, ec, false);
    base.predict(poly.synth_ec);
    label_data *ld = (label_data *) ec.ld;
    if (ld->label != FLT_MAX)
      ec.loss = poly.all->loss->getLoss(poly.all->sd, ld->prediction, ld->label) * ld->weight;
  }

  void learn(stagewise_poly &poly, learner &base, example &ec)
  {
    bool training = poly.all->training && !ec.test_only && ((label_data *) ec.ld)->label != FLT_MAX;

    if (training) {
      synthetic_create(poly, ec, training);
      base.learn(poly.synth_ec);
      ec.loss = poly.synth_ec.loss;

      if (ec.example_counter && poly.batch_sz && !(ec.example_counter % poly.batch_sz))
        sort_data_update_support(poly);
    } else
      predict(poly, base, ec);
  }


#ifdef PARALLEL_ENABLE
  void reduce_min(uint8_t &v1,const uint8_t &v2)
  {
    if(v1 == 0xff)
      v1 = v2;
    else if(v2 != 0xff)
      v1 = (v1 <= v2) ? v1 : v2;
  }

  void sanity_check_state(stagewise_poly &poly)
  {
    for (uint32_t i = 0; i != poly.all->length(); ++i)
    {
      uint32_t wid = stride_shift(poly, i);

      assert( ! cycle_get(poly,wid) );

      assert( ! (min_depths_get(poly, wid) == 0xff && parent_get(poly, wid)) );

      assert( ! (min_depths_get(poly, wid) == 0xff && fabsf(poly.all->reg.weight_vector[wid]) > 0) );
      //assert( min_depths_get(poly, wid) != 0xff && fabsf(poly.all->reg.weight_vector[wid]) < tolerance );

      assert( ! (poly.depthsbits[wid_mask_un_shifted(poly, wid) * 2 + 1] & ~3) );
    }
  }

  void end_pass(stagewise_poly &poly)
  {
    assert(poly.all->span_server == "" || !poly.batch_sz);

    uint64_t sum_sparsity_inc = poly.sum_sparsity - poly.sum_sparsity_sync;
    uint64_t sum_input_sparsity_inc = poly.sum_input_sparsity - poly.sum_input_sparsity_sync;
    uint64_t num_examples_inc = poly.num_examples - poly.num_examples_sync;

    //sanity_check_state(poly);

    vw &all = *poly.all;
    if(all.span_server != "") {
      /*
       * The following is inconsistent with the transplant code in
       * synthetic_create_rec(), which clears parent bits on depth mismatches.
       * But it's unclear what the right behavior is in general for either
       * case...
       */
      all_reduce<uint8_t, reduce_min>(poly.depthsbits, depthsbits_sizeof(poly), all.span_server, all.unique_id, all.total, all.node, all.socks);

      sum_input_sparsity_inc = accumulate_scalar(all, all.span_server, sum_input_sparsity_inc);
      sum_sparsity_inc = accumulate_scalar(all, all.span_server, sum_sparsity_inc);
      num_examples_inc = accumulate_scalar(all, all.span_server, num_examples_inc);
    }

    poly.sum_input_sparsity_sync = poly.sum_input_sparsity_sync + sum_input_sparsity_inc;
    poly.sum_input_sparsity = poly.sum_input_sparsity_sync;
    poly.sum_sparsity_sync = poly.sum_sparsity_sync + sum_sparsity_inc;
    poly.sum_sparsity = poly.sum_sparsity_sync;
    poly.num_examples_sync = poly.num_examples_sync + num_examples_inc;
    poly.num_examples = poly.num_examples_sync;

    //sanity_check_state(poly);

    if (!poly.batch_sz && poly.numpasses != poly.all->numpasses) {
      sort_data_update_support(poly);
      poly.numpasses++;
    }

    //sanity_check_state(poly);
  }

#endif //PARALLEL_ENABLE

  void finish_example(vw &all, stagewise_poly &poly, example &ec)
  {
    size_t temp_num_features = ec.num_features;
    ec.num_features = poly.synth_ec.num_features;
    output_and_account_example(all, ec);
    ec.num_features = temp_num_features;
    VW::finish_example(all, &ec);
  }

  void finish(stagewise_poly &poly)
  {
#ifdef DEBUG
    cout<<"total feature number (after poly expansion!) = " << poly.sum_sparsity << endl;
#endif //DEBUG

    poly.synth_ec.atomics[tree_atomics].delete_v();
    sort_data_destroy(poly);
    depthsbits_destroy(poly);
  }


  void save_load(stagewise_poly &poly, io_buf &model_file, bool read, bool text)
  {
    if (model_file.files.size() > 0)
      bin_text_read_write_fixed(model_file, (char *) poly.depthsbits, depthsbits_sizeof(poly), "", read, "", 0, text);
  }


  learner *setup(vw &all, po::variables_map &vm)
  {
    stagewise_poly *poly = (stagewise_poly *) calloc(1, sizeof(stagewise_poly));
    poly->all = &all;

    depthsbits_create(*poly);
    sort_data_create(*poly);

    po::options_description sp_opt("Stagewise poly options");
    sp_opt.add_options()
      ("sched_exponent", po::value<float>(), "exponent on schedule")
      ("magic_argument", po::value<float>(), "magical feature flag")
      ("batch_sz", po::value<uint32_t>(), "batch size");
    vm = add_options(all, sp_opt);

    poly->sched_exponent = vm.count("sched_exponent") ? vm["sched_exponent"].as<float>() : 0.;
    poly->magic_argument = vm.count("magic_argument") ? vm["magic_argument"].as<float>() : 0.;
    poly->batch_sz = vm.count("batch_sz") ? vm["batch_sz"].as<uint32_t>() : 0;

    poly->sum_sparsity = 0;
    poly->sum_input_sparsity = 0;
    poly->num_examples = 0;
    poly->sum_sparsity_sync = 0;
    poly->sum_input_sparsity_sync = 0;
    poly->num_examples_sync = 0;
    poly->numpasses = 1;

    learner *l = new learner(poly, all.l);
    l->set_learn<stagewise_poly, learn>();
    l->set_predict<stagewise_poly, predict>();
    l->set_finish<stagewise_poly, finish>();
    l->set_save_load<stagewise_poly, save_load>();
    l->set_finish_example<stagewise_poly,finish_example>();
#ifdef PARALLEL_ENABLE
    l->set_end_pass<stagewise_poly, end_pass>();
#endif
    return l;
  }
}
