using System;
using System.Linq;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using ClientDecisionService;
using MultiWorldTesting;
using System.Net;
using System.Threading.Tasks;
using System.Threading;
using System.IO;
using System.Text;
using Microsoft.Research.DecisionService.Common;
using Newtonsoft.Json;
using System.Web;

namespace ClientDecisionServiceTest
{
    [TestClass]
    public class ActionDependentFeaturesTest
    {
        [TestMethod]
        public void TestADFExplorationResult()
        {
            joinServer.Reset();

            var dsConfig = new DecisionServiceConfiguration<TestADFContext>(
                authorizationToken: MockCommandCenter.AuthorizationToken,
                explorer: new EpsilonGreedyExplorer<TestADFContext>(new TestADFPolicy(), epsilon: 0.5f))
            {
                PollingForModelPeriod = TimeSpan.MinValue,
                PollingForSettingsPeriod = TimeSpan.MinValue,
                LoggingServiceAddress = MockJoinServer.MockJoinServerAddress
            };

            var ds = new DecisionService<TestADFContext>(dsConfig);

            string uniqueKey = "eventid";

            for (int i = 1; i <= 100; i++)
            {
                uint[] action = ds.ChooseAction(uniqueKey, new TestADFContext(i));

                Assert.AreEqual(i, action.Length);

                // verify all unique actions in the list
                Assert.AreEqual(action.Length, action.Distinct().Count());

                // verify the actions are in the expected range
                Assert.AreEqual((i * (i + 1)) / 2, action.Sum(a => a));

                ds.ReportReward(i / 100f, uniqueKey);
            }

            ds.Flush();

            Assert.AreEqual(200, joinServer.EventBatchList.Sum(b => b.ExperimentalUnitFragments.Count));
        }

        [TestInitialize]
        public void Setup()
        {
            joinServer = new MockJoinServer(MockJoinServer.MockJoinServerAddress);

            joinServer.Run();
        }

        [TestCleanup]
        public void CleanUp()
        {
            joinServer.Stop();
        }

        private MockJoinServer joinServer;
    }
}
