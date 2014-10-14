#include "stdafx.h"

//
// Top-level internal API for joining reward information to interaction data.
//
class MWTRewardReporter
{
public: 
	MWTRewardReporter(size_t& num_interactions, Interaction* interactions[])
	{
		for (u64 i = 0; i < num_interactions; i++)
		{
			// Datasets returned by MWT apis should not contain null entries, but we check here
			// in case the user modified/mishandled the dataset. 
			if (interactions[i])
			{
				m_interactions[interactions[i]->Get_Id_Hash()] = interactions[i];
			}
		}
	}

	bool ReportReward(std::string unique_id, float reward)
	{
		bool id_present = false;
		u64 id = Interaction::Get_Id_Hash(unique_id);
		if (m_interactions.find(id) != m_interactions.end())
		{
			id_present = true;
			m_interactions[id]->Set_Reward(reward);
		}
		return id_present;
	}

	bool ReportReward(size_t num_entries, std::string unique_ids[], float rewards[])
	{
		bool all_ids_present = false;
		u64 id;
		for (u64 i = 0; i < num_entries; i++)
		{
			all_ids_present &= ReportReward(unique_ids[i], rewards[i]);
		}
		return all_ids_present;
	}

	std::string Get_All_Interactions()
	{
		std::ostringstream serialized_stream;
		for (auto interaction : m_interactions)
		{
			interaction.second->Serialize(serialized_stream);
		}
		return serialized_stream.str();
	}

	//TODO: Add interface to get all interactions as array? How about get all complete interactions? 
	// Or something to set the reward for all incomplete interactions?

private:
	std::map<u64, Interaction*> m_interactions;
};
