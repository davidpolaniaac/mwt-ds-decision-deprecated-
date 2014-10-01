// vw_explore_clr_wrapper.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "vw_explore_clr_wrapper.h"

using namespace System;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;
using namespace msclr::interop;

namespace MultiWorldTesting {

	MWTWrapper::MWTWrapper(System::String^ appId)
	{
		m_mwt = new MWTExplorer(msclr::interop::marshal_as<std::string>(appId));
	}

	MWTWrapper::~MWTWrapper()
	{
		delete m_mwt;
	}

	void MWTWrapper::InitializeEpsilonGreedy(float epsilon, StatefulPolicyDelegate^ defaultPolicyFunc, System::IntPtr defaultPolicyFuncContext, UInt32 numActions)
	{
		GCHandle gch = GCHandle::Alloc(defaultPolicyFunc);
		IntPtr ip = Marshal::GetFunctionPointerForDelegate(defaultPolicyFunc);

		Stateful_Policy_Func* nativeFunc = static_cast<Stateful_Policy_Func*>(ip.ToPointer());
		m_mwt->Initialize_Epsilon_Greedy(epsilon, nativeFunc, defaultPolicyFuncContext.ToPointer(), numActions);

		gch.Free();
	}

	void MWTWrapper::InitializeEpsilonGreedy(float epsilon, StatelessPolicyDelegate^ defaultPolicyFunc, UInt32 numActions)
	{
		GCHandle gch = GCHandle::Alloc(defaultPolicyFunc);
		IntPtr ip = Marshal::GetFunctionPointerForDelegate(defaultPolicyFunc);

		Stateless_Policy_Func* nativeFunc = static_cast<Stateless_Policy_Func*>(ip.ToPointer());
		m_mwt->Initialize_Epsilon_Greedy(epsilon, nativeFunc, numActions);

		gch.Free();
	}

	void MWTWrapper::InitializeTauFirst(UInt32 tau, StatefulPolicyDelegate^ defaultPolicyFunc, System::IntPtr defaultPolicyFuncContext, UInt32 numActions)
	{
		GCHandle gch = GCHandle::Alloc(defaultPolicyFunc);
		IntPtr ip = Marshal::GetFunctionPointerForDelegate(defaultPolicyFunc);

		Stateful_Policy_Func* nativeFunc = static_cast<Stateful_Policy_Func*>(ip.ToPointer());
		m_mwt->Initialize_Tau_First(tau, nativeFunc, defaultPolicyFuncContext.ToPointer(), numActions);

		gch.Free();
	}

	void MWTWrapper::InitializeTauFirst(UInt32 tau, StatelessPolicyDelegate^ defaultPolicyFunc, UInt32 numActions)
	{
		GCHandle gch = GCHandle::Alloc(defaultPolicyFunc);
		IntPtr ip = Marshal::GetFunctionPointerForDelegate(defaultPolicyFunc);

		Stateless_Policy_Func* nativeFunc = static_cast<Stateless_Policy_Func*>(ip.ToPointer());
		m_mwt->Initialize_Tau_First(tau, nativeFunc, numActions);

		gch.Free();
	}

	void MWTWrapper::InitializeBagging(UInt32 bags, cli::array<StatefulPolicyDelegate^>^ defaultPolicyFuncs, cli::array<IntPtr>^ defaultPolicyArgs, UInt32 numActions)
	{
		cli::array<GCHandle>^ gcHandles = gcnew cli::array<GCHandle>(bags);

		Stateful_Policy_Func** native_funcs = new Stateful_Policy_Func*[bags];
		void** native_args = new void*[bags];

		for (int i = 0; i < bags; i++)
		{
			gcHandles[i] = GCHandle::Alloc(defaultPolicyFuncs[i]);
			IntPtr ip = Marshal::GetFunctionPointerForDelegate(defaultPolicyFuncs[i]);

			native_funcs[i] = static_cast<Stateful_Policy_Func*>(ip.ToPointer());
			native_args[i] = defaultPolicyArgs[i].ToPointer();
		}

		m_mwt->Initialize_Bagging(bags, native_funcs, native_args, numActions);

		for (int i = 0; i < bags; i++)
		{
			gcHandles[i].Free();
		}
		delete[] native_funcs;
		delete[] native_args;
	}

	void MWTWrapper::InitializeBagging(UInt32 bags, cli::array<StatelessPolicyDelegate^>^ defaultPolicyFuncs, UInt32 numActions)
	{
		cli::array<GCHandle>^ gcHandles = gcnew cli::array<GCHandle>(bags);

		Stateless_Policy_Func** native_funcs = new Stateless_Policy_Func*[bags];

		for (int i = 0; i < bags; i++)
		{
			gcHandles[i] = GCHandle::Alloc(defaultPolicyFuncs[i]);
			IntPtr ip = Marshal::GetFunctionPointerForDelegate(defaultPolicyFuncs[i]);

			native_funcs[i] = static_cast<Stateless_Policy_Func*>(ip.ToPointer());
		}

		m_mwt->Initialize_Bagging(bags, native_funcs, numActions);

		for (int i = 0; i < bags; i++)
		{
			gcHandles[i].Free();
		}
		delete[] native_funcs;
	}

	void MWTWrapper::InitializeSoftmax(float lambda, StatefulScorerDelegate^ defaultScorerFunc, IntPtr defaultPolicyFuncContext, UInt32 numActions)
	{
		GCHandle gch = GCHandle::Alloc(defaultScorerFunc);
		IntPtr ip = Marshal::GetFunctionPointerForDelegate(defaultScorerFunc);

		Stateful_Scorer_Func* nativeFunc = static_cast<Stateful_Scorer_Func*>(ip.ToPointer());
		m_mwt->Initialize_Softmax(lambda, nativeFunc, defaultPolicyFuncContext.ToPointer(), numActions);

		gch.Free();
	}

	void MWTWrapper::InitializeSoftmax(float lambda, StatelessScorerDelegate^ defaultScorerFunc, UInt32 numActions)
	{
		GCHandle gch = GCHandle::Alloc(defaultScorerFunc);
		IntPtr ip = Marshal::GetFunctionPointerForDelegate(defaultScorerFunc);

		Stateless_Scorer_Func* nativeFunc = static_cast<Stateless_Scorer_Func*>(ip.ToPointer());
		m_mwt->Initialize_Softmax(lambda, nativeFunc, numActions);

		gch.Free();
	}

	UInt32 MWTWrapper::ChooseAction(Context^ context, String^ uniqueId)
	{
		return this->ChooseAction(context->Features, context->OtherContext, uniqueId);
	}

	UInt32 MWTWrapper::ChooseAction(cli::array<FEATURE>^ contextFeatures, String^ otherContext, String^ uniqueId)
	{
		UInt32 chosenAction = 0;

		std::string nativeOtherContext = marshal_as<std::string>(otherContext);
		std::string nativeUniqueKey = marshal_as<std::string>(uniqueId);

		pin_ptr<FEATURE> pinnedContextFeatures = &contextFeatures[0]; 
		FEATURE* nativeContextFeatures = pinnedContextFeatures;

		size_t uniqueIdLength = (size_t)uniqueId->Length;

		chosenAction = m_mwt->Choose_Action(
			(feature*)nativeContextFeatures, (size_t)contextFeatures->Length, 
			&nativeOtherContext, 
			nativeUniqueKey);

		return chosenAction;
	}

	String^ MWTWrapper::GetAllInteractions()
	{
		std::string all_interactions = m_mwt->Get_All_Interactions();
		return gcnew String(all_interactions.c_str());
	}
}