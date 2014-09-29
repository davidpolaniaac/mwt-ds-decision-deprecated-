#pragma once

#include "utility.h"
#include "Interaction.h"

class BaseFunctionWrapper
{ 
public:
	virtual ~BaseFunctionWrapper() { }
};

class MWT_Empty { };

typedef u32 Stateful_Policy_Func(void* state_Context, void* application_Context);
typedef u32 Stateless_Policy_Func(void* application_Context);
//TODO: I think vectors will cause problem with C# interop
typedef std::vector<float> Stateful_Scorer_Func(void* state_Context, void* application_Context);
typedef std::vector<float> Stateless_Scorer_Func(void* application_Context);


template <class T>
class StatefulFunctionWrapper : public BaseFunctionWrapper
{
public:
	typedef u32 Policy_Func(T* state_Context, Context* application_Context);
	typedef std::vector<float> Scorer_Func(T* state_Context, Context* application_Context);

	Stateful_Policy_Func* m_policy_function;
	Stateful_Scorer_Func* m_scorer_function;
};

class StatelessFunctionWrapper : public BaseFunctionWrapper
{
public:
	typedef u32 Policy_Func(Context* application_Context);
	typedef std::vector<float> Scorer_Func(Context* application_Context);

	Stateless_Policy_Func* m_policy_function;
	Stateless_Scorer_Func* m_scorer_function;
};