﻿using DecisionSample;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Newtonsoft.Json;
using System;
using System.IO;

namespace DecisionServiceTest
{
    [TestClass]
    public class Batching
    {
        [TestMethod]
        public void TestBatchingByCount()
        {
            var serviceConfig = new DecisionServiceConfiguration<TestContext>()
            {
                AppId = "mwt",
                Explorer = new EpsilonGreedyExplorer<TestContext>(new TestPolicy(), epsilon: 0.2f, numActions: 10),
                BatchConfig = new BatchingConfiguration()
                {
                    Duration = TimeSpan.FromDays(30),
                    EventCount = 2,
                    BufferSize = 10 * 1024 * 1024
                }
            };

            var service = new DecisionService<TestContext>(serviceConfig);

            string uniqueKey = "eventid";

            service.ChooseAction(uniqueKey, new TestContext());
            service.ReportOutcome("my json outcome", uniqueKey);
            service.ReportReward(0.5f, uniqueKey);

            var batch = JsonConvert.DeserializeObject<EventBatch>(File.ReadAllText(batchOutputFile), new EventJsonConverter());
            Assert.AreEqual(2, batch.Events.Count);
        }

        [TestMethod]
        public void TestBatchingByTime()
        {
            var serviceConfig = new DecisionServiceConfiguration<TestContext>()
            {
                AppId = "mwt",
                Explorer = new EpsilonGreedyExplorer<TestContext>(new TestPolicy(), epsilon: 0.2f, numActions: 10),
                BatchConfig = new BatchingConfiguration()
                {
                    Duration = TimeSpan.FromMilliseconds(100),
                    EventCount = 2,
                    BufferSize = 10 * 1024 * 1024
                }
            };

            var service = new DecisionService<TestContext>(serviceConfig);

            string uniqueKey = "eventid";

            service.ChooseAction(uniqueKey, new TestContext());
            
            System.Threading.Thread.Sleep(200);

            service.ChooseAction(uniqueKey, new TestContext());

            var batch = JsonConvert.DeserializeObject<EventBatch>(File.ReadAllText(batchOutputFile), new EventJsonConverter());
            Assert.AreEqual(1, batch.Events.Count);
        }

        [TestMethod]
        public void TestBatchingBySize()
        {
            var serviceConfig = new DecisionServiceConfiguration<TestContext>()
            {
                AppId = "mwt",
                Explorer = new EpsilonGreedyExplorer<TestContext>(new TestPolicy(), epsilon: 0.2f, numActions: 10),
                BatchConfig = new BatchingConfiguration()
                {
                    Duration = TimeSpan.FromSeconds(1),
                    EventCount = 2,
                    BufferSize = 1
                }
            };

            var service = new DecisionService<TestContext>(serviceConfig);

            string uniqueKey = "eventid";

            service.ChooseAction(uniqueKey, new TestContext());
            System.Threading.Thread.Sleep(500); // sleep to wait for I/O operation to complete
            service.ChooseAction(uniqueKey, new TestContext());

            var batch = JsonConvert.DeserializeObject<EventBatch>(File.ReadAllText(batchOutputFile), new EventJsonConverter());
            Assert.AreEqual(1, batch.Events.Count);
        }

        [TestCleanup]
        public void CleanUp()
        {
            File.Delete(batchOutputFile);
        }

        private readonly string batchOutputFile = "decision_service_test_output";
    }
}
