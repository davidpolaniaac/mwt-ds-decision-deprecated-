using ClientDecisionService;
using Microsoft.Research.DecisionService.Uploader;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Microsoft.WindowsAzure.Storage;
using Microsoft.WindowsAzure.Storage.Blob;
using MultiWorldTesting;
using Newtonsoft.Json;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace ClientDecisionServiceTest
{
    [TestClass]
    public class DecisionServiceUploadTests
    {
        [TestMethod]
        public void TestDSUploadSingleEvent()
        {
            joinServer.Reset();

            string uniqueKey = "test interaction";

            var dsConfig = new DecisionServiceConfiguration<TestContext>(
                authorizationToken: MockCommandCenter.AuthorizationToken,
                explorer: new EpsilonGreedyExplorer<TestContext>(new TestPolicy(), epsilon: 0.2f, numActions: Constants.NumberOfActions));

            dsConfig.LoggingServiceAddress = MockJoinServer.MockJoinServerAddress;

            var ds = new DecisionService<TestContext>(dsConfig);

            uint[] chosenActions = ds.ChooseAction(uniqueKey, new TestContext());

            ds.Flush();

            Assert.AreEqual(1, joinServer.RequestCount);
            Assert.AreEqual(1, joinServer.EventBatchList.Count);
            Assert.AreEqual(1, joinServer.EventBatchList[0].ExperimentalUnitFragments.Count);
            Assert.AreEqual(uniqueKey, joinServer.EventBatchList[0].ExperimentalUnitFragments[0].Id);
            Assert.IsTrue(joinServer.EventBatchList[0].ExperimentalUnitFragments[0].Value.ToLower().Contains("\"a\":[" + string.Join(",", chosenActions) + "],"));
        }

        [TestMethod]
        public void TestDSUploadMultipleEvents()
        {
            joinServer.Reset();

            string uniqueKey = "test interaction";

            var dsConfig = new DecisionServiceConfiguration<TestContext>(
                authorizationToken: MockCommandCenter.AuthorizationToken,
                explorer: new EpsilonGreedyExplorer<TestContext>(new TestPolicy(), epsilon: 0.2f, numActions: Constants.NumberOfActions));

            dsConfig.LoggingServiceAddress = MockJoinServer.MockJoinServerAddress;

            var ds = new DecisionService<TestContext>(dsConfig);

            uint[] chosenAction1 = ds.ChooseAction(uniqueKey, new TestContext());
            uint[] chosenAction2 = ds.ChooseAction(uniqueKey, new TestContext());
            ds.ReportReward(1.0f, uniqueKey);
            ds.ReportOutcome(new { value = "test outcome" }, uniqueKey);

            ds.Flush();

            Assert.AreEqual(4, joinServer.EventBatchList.Sum(batch => batch.ExperimentalUnitFragments.Count));
        }

        [TestMethod]
        public void TestDSUploadSelective()
        {
            joinServer.Reset();

            string uniqueKey = "test interaction";

            var dsConfig = new DecisionServiceConfiguration<TestContext>(
                authorizationToken: MockCommandCenter.AuthorizationToken,
                explorer: new EpsilonGreedyExplorer<TestContext>(new TestPolicy(), epsilon: 0.2f, numActions: Constants.NumberOfActions));

            dsConfig.LoggingServiceAddress = MockJoinServer.MockJoinServerAddress;
            dsConfig.JoinServiceBatchConfiguration = new BatchingConfiguration();
            dsConfig.JoinServiceBatchConfiguration.MaxDuration = TimeSpan.FromMinutes(10); // allow enough time for queue to buffer events
            dsConfig.JoinServiceBatchConfiguration.MaxDegreeOfSerializationParallelism = 1; // single-threaded for easy verification

            int numEvents = 100;

            // Set queue capacity to same number of events so selective dropping starts at 50% full
            dsConfig.JoinServiceBatchConfiguration.MaxUploadQueueCapacity = numEvents;
            dsConfig.JoinServiceBatchConfiguration.DroppingPolicy = new DroppingPolicy 
            {
                SelectiveUploadLevelThreshold = .5f,
                
                // when threshold is reached, drop half of the events
                SelectProbability = .5f 
            };

            var ds = new DecisionService<TestContext>(dsConfig);
            for (int i = 0; i < numEvents; i++)
            {
                uint[] chosenAction1 = ds.ChooseAction(uniqueKey, new TestContext());
            }
            ds.Flush();

            // Some events must have been dropped so the total count cannot be same as original
            Assert.IsTrue(joinServer.EventBatchList.Sum(batch => batch.ExperimentalUnitFragments.Count) < numEvents);

            // Get number of events that have been downsampled, i.e. selected with probability q
            int numSampledEvents = joinServer.EventBatchList
                .SelectMany(batch => batch.ExperimentalUnitFragments.Select(e => e)).Where(e => e.Value.Contains(".0199")).Count();

            Assert.IsTrue(numSampledEvents > 0);

            // half of the events are selected with probability 0.5, so this should definitely be less than half the total events
            Assert.IsTrue(numSampledEvents < numEvents / 2);
        }

        [TestMethod]
        public void TestDSThreadSafeUpload()
        {
            joinServer.Reset();

            string uniqueKey = "test interaction";

            var createObservation = (Func<int, string>)((i) => { return string.Format("00000", i); });

            var dsConfig = new DecisionServiceConfiguration<TestContext>(
                authorizationToken: MockCommandCenter.AuthorizationToken,
                explorer: new EpsilonGreedyExplorer<TestContext>(new TestPolicy(), epsilon: 0.2f, numActions: Constants.NumberOfActions));

            dsConfig.JoinServiceBatchConfiguration = new Microsoft.Research.DecisionService.Uploader.BatchingConfiguration 
            { 
                MaxBufferSizeInBytes = 4 * 1024 * 1024,
                MaxDuration = TimeSpan.FromMinutes(1),
                MaxEventCount = 10000,
                MaxUploadQueueCapacity = 1024 * 32,
                UploadRetryPolicy = Microsoft.Research.DecisionService.Uploader.BatchUploadRetryPolicy.ExponentialRetry,
                MaxDegreeOfSerializationParallelism = Environment.ProcessorCount
            };

            dsConfig.LoggingServiceAddress = MockJoinServer.MockJoinServerAddress;

            var ds = new DecisionService<TestContext>(dsConfig);

            int numEvents = 1000;

            var chosenActions = new ConcurrentBag<uint[]>();

            Parallel.For(0, numEvents, new ParallelOptions { MaxDegreeOfParallelism = Environment.ProcessorCount * 2 }, (i) =>
            {
                chosenActions.Add(ds.ChooseAction(uniqueKey, new TestContext()));
                ds.ReportOutcome(new { value = createObservation(i) }, uniqueKey);
            });

            ds.Flush();

            List<PartialDecisionServiceMessage> batchList = this.joinServer.EventBatchList;
            int numActualEvents = batchList.Sum(b => b.ExperimentalUnitFragments.Count);
            Assert.AreEqual(numEvents * 2, numActualEvents);

            List<string> uniqueKeys = batchList
                .SelectMany(b => b.ExperimentalUnitFragments.Select(f => f.Id))
                .Distinct()
                .ToList();

            Assert.AreEqual(1, uniqueKeys.Count);
            Assert.AreEqual(uniqueKey, uniqueKeys[0]);

            var completeFragments = batchList
                .SelectMany(b => b.ExperimentalUnitFragments
                .Select(f => JsonConvert.DeserializeObject<CompleteExperimentalUnitFragment>(f.Value)));

            // Test actual interactions received 
            List<CompleteExperimentalUnitFragment> interactions = completeFragments
                .Where(f => f.Value == null)
                .OrderBy(f => f.Actions[0])
                .ToList();

            // Test values of the interactions
            Assert.AreEqual(numEvents, interactions.Count);
            var chosenActionList = chosenActions.OrderBy(a => a[0]).ToList();
            for (int i = 0; i < interactions.Count; i++)
            {
                Assert.AreEqual((int)chosenActionList[i][0], interactions[i].Actions[0]);
            }

            // Test actual observations received
            List<CompleteExperimentalUnitFragment> observations = completeFragments
                .Where(f => f.Value != null)
                .OrderBy(f => f.Value)
                .ToList();

            // Test values of the observations
            Assert.AreEqual(numEvents, observations.Count);
            for (int i = 0; i < observations.Count; i++)
            {
                Assert.AreEqual(JsonConvert.SerializeObject(new { value = createObservation(i) }), observations[i].Value);
            }
        }

        [TestInitialize]
        public void Setup()
        {
            commandCenter = new MockCommandCenter(MockCommandCenter.AuthorizationToken);
            joinServer = new MockJoinServer(MockJoinServer.MockJoinServerAddress);

            joinServer.Run();
        }

        [TestCleanup]
        public void CleanUp()
        {
            joinServer.Stop();
        }

        private MockJoinServer joinServer;
        private MockCommandCenter commandCenter;
    }
}
