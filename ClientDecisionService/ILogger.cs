using MultiWorldTesting;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService
{
    public interface ILogger<TContext> : IRecorder<TContext>
    {
        void ReportReward(UniqueEventID uniqueKey, float reward);
        void ReportOutcome(UniqueEventID uniqueKey, object outcome);
        void Flush();
    }
}
