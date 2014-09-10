/*
   Copyright (c) by respective owners including Yahoo!, Microsoft, and
   individual contributors. All rights reserved.  Released under a BSD (revised)
   license as described in the file LICENSE.
   */
#include "searn_dep_parser.h"
#include "multiclass.h"
#include "memory.h"
#include "example.h"
#include "gd.h"
#include "ezexample.h"

#define cdep cerr
#undef cdep
#define cdep if (1) {} else cerr
#define val_namespace 126 // valency and distance feature space
#define offset_const 79867

namespace DepParserTask         {  Searn::searn_task task = { "dep_parser", initialize, finish, structured_predict };  }

struct task_data {
	example *ex;
	bool no_quadratic_features;
	bool no_cubic_features;
	bool my_init_flag;
	v_array<uint32_t> valid_actions;
	v_array<uint32_t> gold_heads; // gold labels
	v_array<uint32_t> gold_actions;
	v_array<uint32_t> *children; // [0]:num_left_arcs, [1]:num_right_arcs; [2]: leftmost_arc, [3]: second_leftmost_arc, [4]:rightmost_arc, [5]: second_rightmost_arc
	v_array<uint32_t> stack; // stack for transition based parser
	v_array<uint32_t> heads; // output array
	v_array<uint32_t> temp;
	v_array<example *> ec_buf;
};

namespace DepParserTask {
	using namespace Searn;
	uint32_t max_label = 0;

	void initialize(searn& srn, size_t& num_actions, po::variables_map& vm) {
		task_data *data = new task_data();
		data->my_init_flag = false;
		data->ex = (example*)calloc_or_die(1, sizeof(example));
		data->ec_buf.resize(12, true);
		data->children = new v_array<uint32_t>[6]; 


		srn.set_options(0);
		srn.set_num_learners(1);
		srn.set_task_data<task_data>(data);
		po::options_description sspan_opts("dependency parser options");
		sspan_opts.add_options()
			("dparser_no_quad", "Don't use qudaratic features")
			("dparser_no_cubic","Don't use cubic features");
		vm = add_options(*srn.all, sspan_opts);

		// setup entity and relation labels
		// Entity label 1:E_Other 2:E_Peop 3:E_Org 4:E_Loc
		// Relation label 5:R_Live_in 6:R_OrgBased_in 7:R_Located_in 8:R_Work_For 9:R_Kill 10:R_None
		data->no_quadratic_features = (vm.count("dparser_no_quad"))?true:false;
		data->no_cubic_features =(vm.count("dparser_no_cubic"))?true:false;
	}

	void finish(searn& srn) {
		task_data *data = srn.get_task_data<task_data>();
		dealloc_example(srn.all->p->lp.delete_label, *(data->ex));
		data->valid_actions.delete_v();
		data->gold_heads.delete_v();
		data->gold_actions.delete_v();
		data->stack.delete_v();
		data->heads.delete_v();
		data->ec_buf.delete_v();
		data->temp.delete_v();
		for(size_t i=0; i<6; i++)
			data->children[i].delete_v();
		delete[] data->children;
		delete data;
	} // if we had task data, we'd want to free it here

	// arc-hybrid System.
	uint32_t transition_hybrid(searn& srn, uint32_t a_id, uint32_t idx) {
		task_data *data = srn.get_task_data<task_data>();
		v_array<uint32_t> &heads=data->heads, &stack=data->stack, &gold_heads=data->gold_heads;
		v_array<uint32_t> *children = data->children;
		switch(a_id) {
			//SHIFT
			case 1:
				stack.push_back(idx);
				return idx+1;

				//RIGHT
			case 2:
				heads[stack.last()] = stack[stack.size()-2];
				cdep << "make a right link" << stack[stack.size()-2] << " ====> " << (stack.last()) << endl;
				srn.loss((gold_heads[stack.last()] != heads[stack.last()])/((float)gold_heads.size()));
				children[5][stack[stack.size()-2]]=children[4][stack[stack.size()-2]];
				children[4][stack[stack.size()-2]]=stack.last();
				children[1][stack[stack.size()-2]]++;
				stack.pop();
				return idx;

				//LEFT
			case 3:
				heads[stack.last()] = idx;
				cdep << "make a left link" << stack.last() << "<==== " << idx << endl;
				srn.loss((gold_heads[stack.last()] != heads[stack.last()])/((float)gold_heads.size()));
				children[3][idx]=children[2][idx];
				children[2][idx]=stack.last();
				children[0][idx]++;
				stack.pop();
				return idx;
		}
		cerr << "Unknown action (searn_dep_parser.cc).";
		return idx;
	}
	void check_feature_vector(example *ec){
		float total_fs = 0.0;
		int total_num_features = 0;
		for (unsigned char* j = ec->indices.begin,fs_idx_inner = 0; j != ec->indices.end; j++,fs_idx_inner++) {
			for(size_t k=0; k<ec->atomics[*j].size(); k++) {
				total_num_features+=1;
				total_fs += 1;
			}
		}

		if(ec->total_sum_feat_sq != total_fs){
			cerr<< ec->total_sum_feat_sq<<" " << total_fs <<endl;
			cerr<< ec->num_features <<" "<< total_num_features<<endl;
		}

	}

	// This function will only be called once
	// We use VW's internal implementation to create second-order and third-order features
	void my_initialize(searn& srn, example *base_ex) {
		task_data *data = srn.get_task_data<task_data>();
		size_t num_base_feature_space = base_ex->indices.size();


		// setup example
		example *ex = data->ex;
		size_t nfs = base_ex->indices.size();
		uint64_t offset = offset_const;
		for (size_t i=0; i<12; i++) {
			offset= (offset*offset_const) & srn.all->reg.weight_mask;
			unsigned char j = 0;
			for (unsigned char* fs = base_ex->indices.begin; fs != base_ex->indices.end; fs++) {
				if(*fs == constant_namespace) // ignore constant_namespace
					continue;
				ex->indices.push_back(i*nfs+j);				
				for (size_t k=0; k<base_ex->atomics[*fs].size(); k++) {
					uint32_t idx = (offset + j * ex->atomics[i*nfs+j].size() + k + affix_constant) & srn.all->reg.weight_mask;
					feature f = {1.0f, idx};
					ex->atomics[i*nfs+j].push_back(f);
					ex->total_sum_feat_sq += 1.0f;
					ex->num_features++;
					ex->sum_feat_sq[i*nfs+j] += 1.0f;
					if(srn.all->audit){
						cerr << base_ex->atomics[*fs][k].weight_index<<endl;
						audit_data a_feature = {NULL, NULL, (uint32_t) ((offset + ex->atomics[i*nfs+j].size()*j +k + affix_constant) & srn.all->reg.weight_mask), 1.0f, true};
						a_feature.space = (char*)calloc_or_die(1,sizeof(char));
						a_feature.feature = (char*)calloc_or_die(30,sizeof(char));
						sprintf(a_feature.feature, "%d,%d,%d=%d", (int)i, (int)j, (int)k, (int)idx);
						ex->audit_features[i*nfs+j].push_back(a_feature);
					}
				}
				j++;
			}
		}
		ex->indices.push_back(val_namespace);

		// add valency and distance features
		for(int i=0; i<4; i++){
			offset= (offset*offset_const) & srn.all->reg.weight_mask;
			feature f = {1.0f, (uint32_t) offset};
			ex->atomics[val_namespace].push_back(f);
			if(srn.all->audit){
				audit_data a_feature = {NULL, NULL, (uint32_t) offset, 1.0f, true};
				a_feature.space = (char*)calloc_or_die(1,sizeof(char));
				a_feature.feature = (char*)calloc_or_die(30,sizeof(char));
				sprintf(a_feature.feature, "%d=%d", (int)i, 0);
				ex->audit_features[val_namespace].push_back(a_feature);
			}

		}
		ex->num_features+=4;
		ex->sum_feat_sq[val_namespace] += 4.0f;
		ex->total_sum_feat_sq += 4.0f;

		// add constant
		if (srn.all->add_constant) {
			VW::add_constant_feature(*(srn.all), ex);
		}

		// setup feature template
		vector<string> newpairs;
		vector<string> newtriples;
		map<string, char> fs_idx_map;
		fs_idx_map["s1"]=0, fs_idx_map["s2"]=1, fs_idx_map["s3"]=2;
		fs_idx_map["b1"]=3, fs_idx_map["b2"]=4, fs_idx_map["b3"]=5;
		fs_idx_map["sl1"]=6, fs_idx_map["sl2"]=7, fs_idx_map["sr1"]=8;
		fs_idx_map["sr2"]=9, fs_idx_map["bl1"]=10, fs_idx_map["br2"]=11;

		size_t pos = 0;

		if(!data->no_quadratic_features){
			// features based on context
			string quadratic_feature_template = "s1-s2 s1-b1 s1-s1 s2-s2 s3-s3 b1-b1 b2-b3 b3-b3 ENDQ";
			// Generate quadratic features
			while ((pos = quadratic_feature_template.find(" ")) != std::string::npos) {
				string token = quadratic_feature_template.substr(0, pos);
				char first_fs_idx = fs_idx_map[token.substr(0,token.find("-"))];
				char second_fs_idx = fs_idx_map[token.substr(token.find("-")+1,token.size())];
				for (size_t i=0; i<num_base_feature_space; i++) {
					for (size_t j=0; j<num_base_feature_space; j++) {
						char space_a = (char)(first_fs_idx*num_base_feature_space+i);
						char space_b = (char)(second_fs_idx*num_base_feature_space+j);
						newpairs.push_back(string(1, space_a)+ string(1, space_b));
						ex->num_features 
							+= (ex->atomics[(int)space_a].end - ex->atomics[(int)space_a].begin)
							*(ex->atomics[(int)space_b].end - ex->atomics[(int)space_b].begin);
						ex->total_sum_feat_sq += ex->sum_feat_sq[(int)space_a]*ex->sum_feat_sq[(int)space_b];
					}
				}
				quadratic_feature_template.erase(0, pos + 1);
			}
			/*
			for(size_t i=0; i<12; i++){
					for (size_t j=0; j<num_base_feature_space; j++) {
						char space_a = (char)(val_namespace);
						char space_b = (char)(i*num_base_feature_space+j);
						newpairs.push_back(string(1, space_a)+ string(1, space_b));
						ex->num_features 
							+= (ex->atomics[(int)space_b].end - ex->atomics[(int)space_b].begin);
						ex->total_sum_feat_sq += ex->sum_feat_sq[(int)space_b];

					}
			}*/
			srn.all->pairs.swap(newpairs);
		}

		// Generate cubic features

		if(!data->no_cubic_features){
			string cubic_feature_template = "b1-b2-b3 s1-b1-b2 s1-s2-b1 s1-s2-s3 ENDC";
			while ((pos = cubic_feature_template.find(" ")) != std::string::npos) {
				string token = cubic_feature_template.substr(0, pos);
				char first_fs_idx = fs_idx_map[token.substr(0,token.find("-"))];
				token.erase(0, token.find("-")+1);
				char second_fs_idx = fs_idx_map[token.substr(0,token.find("-"))];
				char third_fs_idx = fs_idx_map[token.substr(token.find("-")+1,token.size())];
				for (size_t i=0; i<num_base_feature_space; i++) {
					for (size_t j=0; j<num_base_feature_space; j++) {
						for (size_t k=0; k<num_base_feature_space; k++) {
							char space_a = (char)(first_fs_idx*num_base_feature_space+i);
							char space_b = (char)(second_fs_idx*num_base_feature_space+j);
							char space_c = (char)(third_fs_idx*num_base_feature_space+k);
							newtriples.push_back(string(1,space_a)+string(1, space_b)+string(1, space_c));
							ex->num_features 
								+= (ex->atomics[(int)space_a].end - ex->atomics[(int)space_a].begin)
								*(ex->atomics[(int)space_b].end - ex->atomics[(int)space_b].begin)
								*(ex->atomics[(int)space_c].end - ex->atomics[(int)space_c].begin);
							ex->total_sum_feat_sq += ex->sum_feat_sq[(int)space_a]*ex->sum_feat_sq[(int)space_b]*ex->sum_feat_sq[(int)space_c];

						}
					}
				}
				cubic_feature_template.erase(0, pos + 1);
			}
			srn.all->triples.swap(newtriples);
		}
	}

	// This function needs to be very fast
	void extract_features(searn& srn, uint32_t idx,  vector<example*> &ec) {
		task_data *data = srn.get_task_data<task_data>();
		v_array<uint32_t> &stack = data->stack;
		v_array<uint32_t> *children = data->children;
		v_array<example*> &ec_buf = data->ec_buf;

		example &ex = *(data->ex);

		// be careful: indices in ec starts from 0, but i is starts from 1
		size_t n = ec.size();
		// use this buffer to collect the examples, default value: NULL
		ec_buf.resize(12,true);
		for(size_t i=0; i<12; i++)
			ec_buf[i] = 0;

		// feature based on top three examples in stack
		// ec_buf[0]: s1, ec_buf[1]: s2, ec_buf[2]: s3
		for(size_t i=0; i<3; i++)
			ec_buf[i] = (stack.size()>i) ? ec[*(stack.end-(i+1))-1] : 0;

		// features based on examples in string buffer
		// ec_buf[3]: b1, ec_buf[4]: b2, ec_buf[5]: b3
		for(size_t i=3; i<6; i++)
			ec_buf[i] = (idx+(i-3) < n) ? ec[idx+i-3] : 0;

		// features based on leftmost and rightmost children of the top element stack
		// ec_buf[6]: sl1, ec_buf[7]: sl2, ec_buf[8]: sr1, ec_buf[9]: sr2;

		for(size_t i=6; i<10; i++)
			ec_buf[i] = (!stack.empty() && children[i-4][stack.last()]!=0)? ec[children[i-4][stack.last()]-1] : 0;

		// features based on leftmost children of the top element in bufer
		// ec_buf[10]: bl1, ec_buf[11]: bl2
		for(size_t i=10; i<12; i++)
			ec_buf[i] = (idx <=n && children[i-8][idx]!=0)? ec[children[i-8][idx]-1] : 0;

		size_t dis = (!stack.empty() && idx <=n) ? min(5, idx - stack.last()) : 0;

		cdep << "start generating features";

		// unigram features
		size_t nfs = ec[0]->indices.size();
		uint64_t offset = (uint64_t)offset_const;

		for(size_t i=0; i<12; i++) {
			offset= (offset*offset_const) & srn.all->reg.weight_mask;
			if(!ec_buf[i]) {
				unsigned char j=0;
				for (unsigned char* fs = ec[0]->indices.begin; fs != ec[0]->indices.end; fs++) {
					if(*fs == constant_namespace) // ignore constant_namespace
						continue;
					for(size_t k=0; k<ex.atomics[i*nfs+j].size(); k++) {
						// use affix_constant to represent the features that not appear
						ex.atomics[i*nfs+j][k].weight_index = (uint32_t) ((affix_constant + j*ex.atomics[i*nfs+j].size()+k + offset) & srn.all->reg.weight_mask);
					}
					j++;
				}
			} else {
				unsigned char j = 0;
				for (unsigned char* fs= ec[0]->indices.begin; fs != ec[0]->indices.end; fs++) {
					if(*fs == constant_namespace) // ignore constant_namespace
						continue;
					for(size_t k=0; k<ex.atomics[i*nfs+j].size(); k++) {
						ex.atomics[i*nfs+j][k].weight_index = (uint32_t) ((ec_buf[i]->atomics[*fs][k].weight_index + offset) & srn.all->reg.weight_mask);
					}
					j++;
				}
			}
		}
		offset= (offset*offset_const) & srn.all->reg.weight_mask;
		ex.atomics[val_namespace][0].weight_index = (offset + dis) & srn.all->reg.weight_mask;
		offset= (offset*offset_const) & srn.all->reg.weight_mask;
		ex.atomics[val_namespace][1].weight_index = (offset + (stack.empty() ? 0: min(5,children[0][stack.last()]))) & srn.all->reg.weight_mask;
		offset= (offset*offset_const) & srn.all->reg.weight_mask;
		ex.atomics[val_namespace][2].weight_index = (offset + (stack.empty()? 0 : min(5, children[1][stack.last()]))) & srn.all->reg.weight_mask;
		offset= (offset*constant) & srn.all->reg.weight_mask;
		ex.atomics[val_namespace][3].weight_index = (offset + (idx>n?0:min(5 , children[0][idx]))) & srn.all->reg.weight_mask;

		if(srn.all->audit){
			uint64_t offset = (uint64_t)offset_const;
			for(size_t i=0; i<12; i++) {
				offset= (offset*offset_const) & srn.all->reg.weight_mask;
				if(!ec_buf[i]) {
					for (unsigned char* j = ec[0]->indices.begin,fs_idx_inner = 0; j != ec[0]->indices.end; j++,fs_idx_inner++) {
						if(*j == constant_namespace) // ignore constant_namespace
							continue;
						for(size_t k=0; k<ex.atomics[i*nfs+fs_idx_inner].size(); k++) {
							ex.audit_features[i*nfs+fs_idx_inner][k].weight_index = (uint32_t) ((affix_constant + fs_idx_inner*ex.atomics[i*nfs+fs_idx_inner].size()+k + offset) & srn.all->reg.weight_mask);
							sprintf(ex.audit_features[i*nfs+fs_idx_inner][k].feature, "%d,%d,%d=null", (int)i, (int)fs_idx_inner, (int)k);
						}
					}
				} else {
					unsigned char j = 0;
					for (unsigned char* fs= ec[0]->indices.begin; fs != ec[0]->indices.end; fs++) {
						if(*fs == constant_namespace) // ignore constant_namespace
							continue;
						for(size_t k=0; k<ex.atomics[i*nfs+j].size(); k++) {
							ex.audit_features[i*nfs+j][k].weight_index = (uint32_t) ((ec_buf[i]->atomics[*fs][k].weight_index + offset) & srn.all->reg.weight_mask);
							sprintf(ex.audit_features[i*nfs+j][k].feature, "%d,%d,%d=%d", (int)i, (int)j, (int)k, (int)ec_buf[i]->atomics[*fs][k].weight_index);
						}
					}
					j++;
				}
			}
			offset= (offset*offset_const) & srn.all->reg.weight_mask;
			ex.audit_features[val_namespace][0].weight_index = offset + dis;
			sprintf(ex.audit_features[val_namespace][0].feature, "0=%d", (int)dis);
			ex.audit_features[val_namespace][1].weight_index = offset + children[0][stack.last()];
			sprintf(ex.audit_features[val_namespace][1].feature, "1=%d", (int)min(5,children[0][stack.last()]));
			ex.audit_features[val_namespace][2].weight_index = offset + children[1][stack.last()];
			sprintf(ex.audit_features[val_namespace][2].feature, "2=%d", (int)min(5,children[1][stack.last()]));
			ex.audit_features[val_namespace][3].weight_index = offset + children[0][idx];
			sprintf(ex.audit_features[val_namespace][3].feature, "3=%d", (int)min(5,children[0][idx]), offset, ((ex.audit_features[val_namespace][3].weight_index >> srn.all->reg.stride_shift) & srn.all->parse_mask));
		}
	}

	void get_valid_actions(v_array<uint32_t> & valid_action, uint32_t idx, uint32_t n, uint32_t stack_depth) {
		valid_action.erase();
		// SHIFT
		if(idx<=n)
			valid_action.push_back(1);

		// RIGHT
		if(stack_depth >=2)
			valid_action.push_back(2);

		// LEFT
		if(stack_depth >=1 && idx<=n)
			valid_action.push_back(3);
	}

	bool is_valid(uint32_t action, v_array<uint32_t> valid_actions) {
		for(size_t i=0; i< valid_actions.size(); i++)
			if(valid_actions[i] == action)
				return true;
		return false;
	}

	bool has_dependency(uint32_t target, v_array<uint32_t> others, v_array<uint32_t> gold_heads) {
		for(uint32_t idx = 0; idx<others.size(); idx++)
			if(gold_heads[others[idx]] == target || gold_heads[target] == others[idx])
				return true;
		return false;
	}

	void get_gold_actions(searn &srn, uint32_t idx, uint32_t n){
		task_data *data = srn.get_task_data<task_data>();
		v_array<uint32_t> &gold_actions = data->gold_actions, &stack = data->stack, &gold_heads=data->gold_heads, &valid_actions=data->valid_actions, &temp=data->temp;
		gold_actions.erase();
		cdep << "valid_action=[";for(size_t i=0; i<valid_actions.size(); i++){cdep << valid_actions[i] << " ";}cdep << "]";
		cdep << is_valid(2,valid_actions);
		// gold = SHIFT
		if (is_valid(1,valid_actions) && (stack.empty() || gold_heads[idx] == stack.last())) {
			gold_actions.push_back(1);
			return;
		}

		// gold = LEFT
		if (is_valid(3,valid_actions) && gold_heads[stack.last()] == idx) {
			gold_actions.push_back(3);
			return;
		}

		// gold contains SHIFT
		if (is_valid(1,valid_actions) && !has_dependency(idx, stack, gold_heads))
			gold_actions.push_back(1);

		// gold contains LEFT or RIGHT
		temp.erase();
		for(uint32_t i=idx+1; i<n; i++)
			temp.push_back(i);

		if (!has_dependency(stack.last(), temp, gold_heads)) {
			if (is_valid(2,valid_actions))
				gold_actions.push_back(2);
			if (is_valid(3,valid_actions) && !(stack.size()>=2 && gold_heads[stack.last()] == stack[stack.size()-2]))
				gold_actions.push_back(3);
		}
	}

	void structured_predict(searn& srn, vector<example*>& ec) {
		cdep << "start structured predict"<<endl;
		task_data *data = srn.get_task_data<task_data>();
		v_array<uint32_t> &gold_actions = data->gold_actions, &stack = data->stack, &gold_heads=data->gold_heads, &valid_actions=data->valid_actions, &heads=data->heads;
		uint32_t n = ec.size();
		uint32_t idx = 2;

		// initialization
		if(!data->my_init_flag) {
			my_initialize(srn, ec[0]);
			data->my_init_flag = true;
		}

		heads.resize(ec.size()+1, true);
		gold_heads.erase();
		gold_heads.push_back(0);
		for(size_t i=0; i<ec.size(); i++) {
			gold_heads.push_back(MULTICLASS::get_example_label(ec[i])-1);
			heads[i+1] = 0;
		}
		stack.erase();
		stack.push_back(1);
		for(size_t i=0; i<6; i++){
			data->children[i].resize(ec.size()+1, true);
			for(size_t j=0; j<ec.size()+1; j++) {
				data->children[i][j] = 0;
			}
		}

		int count=0;
		cdep << "start decoding"<<endl;
		while(stack.size()>1 || idx <= n){
			srn.snapshot(count, 1, &count, sizeof(count), true);
			srn.snapshot(count, 2, &idx, sizeof(idx), true);
			srn.snapshot(count, 3, &stack, sizeof(stack[0])*stack.size(), true);
			srn.snapshot(count, 4, &heads, sizeof(heads[0])*n, true);
			cdep << "before transition: idx=" << idx << " n=" << n << " ";
			cdep << "stack = [";for(size_t i=0; i<stack.size(); i++){cdep << stack[i] << " ";} cdep << "]" << endl;
			cdep << "buffer = [";for(size_t i=idx; i<=ec.size(); i++){cdep << i << " ";} cdep << "]" << endl;
			cdep << "heads:[";for(size_t i=0; i<ec.size()+1; i++){cdep << heads[i] << " ";}cdep <<"]"<<endl;
			cdep << "extracting features"<<endl;
			extract_features(srn, idx, ec);

			cdep << "setup valid and gold actions"<<endl;
			get_valid_actions(valid_actions, idx, n, stack.size());
			get_gold_actions(srn, idx, n);
			cdep << "valid_action=[";for(size_t i=0; i<valid_actions.size(); i++){cdep << valid_actions[i] << " ";}cdep << "]";
			cdep << "gold_action=["; for(size_t i=0; i<gold_actions.size(); i++){cdep << gold_actions[i] << " ";} cdep << "]"<<endl;
			cdep << "make prediction"<<endl;
			uint32_t prediction = srn.predict(data->ex, gold_actions[0], &valid_actions);
			idx = transition_hybrid(srn, prediction, idx);
			cdep << "after taking action"<<prediction << " idx="<<idx <<" stack = [";for(size_t i=0; i<stack.size(); i++){cdep << stack[i] << " ";}cdep <<"]"<<endl;
			cdep << "stack = [";for(size_t i=0; i<stack.size(); i++){cdep << stack[i] << " ";} cdep << "]" << endl;
			cdep << "buffer = [";for(size_t i=idx; i<=ec.size(); i++){cdep << i << " ";} cdep << "]" << endl;
			cdep << "heads:[";for(size_t i=0; i<ec.size()+1; i++){cdep << heads[i] << " ";}cdep <<"]"<<endl;
			cdep << "gold_heads:[";for(size_t i=0; i<ec.size()+1; i++){cdep << gold_heads[i] << " ";}cdep <<"]"<<endl;
			cdep << endl;

			count++;
		}
		heads[stack.last()] = 0;
		cdep << "root link to the last element in stack" <<  "root ====> " << (stack.last()) << endl;
		srn.loss((gold_heads[stack.last()] != heads[stack.last()])/((float)gold_heads.size()));
		if (srn.output().good())
			for(size_t i=1; i<=n; i++) {
				cdep << heads[i] << " ";
				srn.output() << heads[i] << " ";
			}
		cdep << "end structured predict"<<endl;
	}
}
