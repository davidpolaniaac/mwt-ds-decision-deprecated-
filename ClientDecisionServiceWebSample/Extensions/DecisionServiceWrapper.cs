using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using MultiWorldTesting;
using ClientDecisionService;

namespace ClientDecisionServiceWebSample.Extensions
{
    public class SampleContext : IContext
    {
        public string Data { get; set; }

        public object GetGlobalFeatures()
        {
            return null;
        }

        public object GetActionFeatures(int action)
        {
            return null;
        }

        public int GetNumberOfActions()
        {
            return 0;
        }

        public string ToVWString()
        {
            return string.Empty;
        }
    }

    public static class DecisionServiceWrapper<TContext>
        where TContext : IContext
    {
        public static EpsilonGreedyExplorer<TContext> Explorer { get; set; }
        public static DecisionServiceConfiguration<TContext> Configuration { get; set; }
        public static DecisionService<TContext> Service { get; set; }

        public static void Create(string appToken, float epsilon, string modelOutputDir)
        {
            if (Explorer == null)
            {
                Explorer = new EpsilonGreedyExplorer<TContext>(new MartPolicy<TContext>(), epsilon);
            }

            if (Configuration == null)
            {
                Configuration = new DecisionServiceConfiguration<TContext>(appToken, Explorer)
                {
                    BlobOutputDir = modelOutputDir,
                    BatchConfig = new BatchingConfiguration
                    {
                        MaxDuration = TimeSpan.FromSeconds(5),
                        MaxBufferSizeInBytes = 10,
                        MaxEventCount = 1,
                        MaxUploadQueueCapacity = 1,
                        UploadRetryPolicy = BatchUploadRetryPolicy.Retry
                    }
                };
            }

            if (Service == null)
            {
                Service = new DecisionService<TContext>(Configuration);
            }
        }
    }

    class MartPolicy<TContext> : IPolicy<TContext>
    {
        public uint ChooseAction(TContext context, uint numActions)
        {
            return 5;
        }
    }
}