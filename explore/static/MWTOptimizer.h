#pragma once

#include "Common.h"
#include "vwdll.h"

//
// Top-level internal API for offline evaluation/optimization.
//
class MWTOptimizer
{
public:
	MWTOptimizer(size_t num_interactions, Interaction* interactions[], u32 num_actions)
		: m_num_actions(num_actions), m_stateful_default_policy_func(nullptr),
		m_stateless_default_policy_func(nullptr), m_default_policy_params(nullptr)
	{
		//TODO: Accept an ActionSet param that we'll use to call Match()? Maybe we should just accept a 
		// Match() method

		for (u64 i = 0; i < num_interactions; i++)
		{
			// Datasets returned by MWT apis should not contain null entries, but check for them
			// just in case; also check for incomplete interactions (currently those without rewards)
			if (interactions[i] && (interactions[i]->Get_Reward() != NO_REWARD))
			{
				m_interactions.push_back(interactions[i]);
			}
		}
		m_num_actions = num_actions;
	}

	template <class T>
	float Evaluate_Policy(
		typename StatefulFunctionWrapper<T>::Policy_Func policy_func,
		T* policy_params)
	{
	  return this->Evaluate_Policy((Stateful_Policy_Func*)policy_func, (void*)policy_params);
	}

	float Evaluate_Policy(StatelessFunctionWrapper::Policy_Func policy_func)
	{
	  return this->Evaluate_Policy((Stateless_Policy_Func*)policy_func);
	}

	float Evaluate_Policy_VW_CSOAA(std::string model_input_file)
	{
		VW_HANDLE vw;
		VW_EXAMPLE example;
		double sum_weighted_rewards = 0.0;
		u64 count = 0;

		std::string params = "-t -i " + model_input_file + " --noconstant --quiet";
		vw = VW_InitializeA(params.c_str());
		MWTAction policy_action(0);
		for (auto pInteraction : m_interactions)
		{
			std::ostringstream serialized_stream;
			pInteraction->Serialize_VW_CSOAA(serialized_stream);
			example = VW_ReadExampleA(vw, serialized_stream.str().c_str());
			policy_action = MWTAction((u32)VW_Predict(vw, example));
			// If the policy action matches the action logged in the interaction, include the
			// (importance-weighted) reward in our average
			MWTAction a = pInteraction->Get_Action();
			if (policy_action.Match(a))
			{
				sum_weighted_rewards += pInteraction->Get_Reward() * (1.0 / pInteraction->Get_Prob());
				count++;
			}
		}
		VW_Finish(vw);

		float expected_perf = (count > 0) ? (sum_weighted_rewards / count) : 0.0;
		return expected_perf;
	}

	void Optimize_Policy_VW_CSOAA(std::string model_output_file)
	{
		VW_HANDLE vw;
		VW_EXAMPLE example;

		std::string params = "--csoaa " + std::to_string(m_num_actions) + " --noconstant --quiet -f " + model_output_file;
		vw = VW_InitializeA(params.c_str());
		for (auto pInteraction : m_interactions)
		{
			std::ostringstream serialized_stream;
			pInteraction->Serialize_VW_CSOAA(serialized_stream);
			example = VW_ReadExampleA(vw, serialized_stream.str().c_str());	
			(void)VW_Learn(vw, example);
		}
		VW_Finish(vw);
	}

public:
	float Evaluate_Policy(
		Stateful_Policy_Func policy_func,
		void* policy_params)
	{
		m_stateful_default_policy_func = policy_func;
		m_stateless_default_policy_func = nullptr;
		m_default_policy_params = policy_params;

		return Evaluate_Policy();
	}

	float Evaluate_Policy(Stateless_Policy_Func policy_func)
	{
		m_stateful_default_policy_func = nullptr;
		m_stateless_default_policy_func = policy_func;

		return Evaluate_Policy();
	}

private:
	float Evaluate_Policy()
	{
		double sum_weighted_rewards = 0.0;
		u64 count = 0;

		for (auto pInteraction : m_interactions)
		{
			MWTAction policy_action(0);
			if (m_stateless_default_policy_func != nullptr)
			{
				policy_action = MWTAction(m_stateless_default_policy_func(pInteraction->Get_Context()));
			}
			else
			{
				policy_action = MWTAction(m_stateful_default_policy_func(m_default_policy_params, pInteraction->Get_Context()));
			}
			// If the policy action matches the action logged in the interaction, include the
			// (importance-weighted) reward in our average
			MWTAction a = pInteraction->Get_Action();
			if (policy_action.Match(a))
			{
				sum_weighted_rewards += pInteraction->Get_Reward() * (1.0 / pInteraction->Get_Prob());
				count++;
			}
		}

		float expected_perf = (count > 0) ? (sum_weighted_rewards / count) : 0.0;
		return expected_perf;
	}


private:
	std::vector<Interaction*> m_interactions;
	u32 m_num_actions;

	Stateful_Policy_Func* m_stateful_default_policy_func;
	Stateless_Policy_Func* m_stateless_default_policy_func;
	void* m_default_policy_params;
};
