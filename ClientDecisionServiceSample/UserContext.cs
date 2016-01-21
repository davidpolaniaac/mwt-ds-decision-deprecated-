using MultiWorldTesting;
using System;
using System.Collections.Generic;
using System.Linq;

namespace ClientDecisionServiceSample
{
    class UserContext
    {

        public UserContext() : this(null) { }

        public UserContext(IDictionary<string, float> features)
        {
            FeatureVector = features;
        }

        public IDictionary<string, float> FeatureVector { get; set; }
    }

    class MyAzureRecorder : IRecorder<UserContext>
    {
        public void Record(UserContext context, UInt32[] action, float probability, UniqueEventID uniqueKey)
        {
            // Stores the tuple in Azure.
        }
    }

    class UserPolicy : IPolicy<UserContext>
    {
        public UserPolicy(int numActions)
        {
            this.numActions = numActions;
        }

        public uint[] ChooseAction(UserContext context)
        {
            return Enumerable.Range(1, numActions).Select(a => (uint)a).ToArray(); //((context.FeatureVector.Length % 2) + 1);
        }

        int numActions;
    }

    class UserScorer : IScorer<UserContext>
    {
        public List<float> ScoreActions(UserContext context)
        {
            return new List<float>();
        }
    }

    class Parsed
    {
        internal string UniqueId { get; set; }

        internal UserContext Context { get; set; }

        internal int TrueAction { get; set; }
    }
}
