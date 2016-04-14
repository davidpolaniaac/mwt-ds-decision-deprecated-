﻿using Microsoft.Research.MultiWorldTesting.ClientLibrary;
using Microsoft.Research.MultiWorldTesting.ExploreLibrary;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using VW;

namespace ClientDecisionServiceTest
{
    [TestClass]
    public class InitialFullExploration : MockCommandTestBase
    {
        private class MyRecorder : IRecorder<string, int[]>
        {
            public GenericExplorerState LastExplorerState { get; set; }

            public object LastMapperState { get; set; }

            public void Record(string context, int[] value, object explorerState, object mapperState, UniqueEventID uniqueKey)
            {
                this.LastExplorerState = (GenericExplorerState)explorerState;
                this.LastMapperState = mapperState;
            }
        }

        [TestMethod]
        public void InitialFullExplorationTest()
        {
            var recorder = new MyRecorder();

            using (var model = new MemoryStream())
            {
                using (var vw = new VowpalWabbit("--cb_adf --rank_all"))
                {
                    vw.ID = "123";
                    vw.SaveModel(model);
                }

                var config = new DecisionServiceConfiguration("") { OfflineMode = true };

                using (var ds =
                        DecisionService
                            .WithRanker(config)
                            .WithJson()
                            .WithTopSlotEpsilonGreedy(0.3f)
                            .WithRecorder(recorder)
                            .ExploreTopSlotUniformRandomUntilModelReady())
                {
                    var decision = ds.ChooseAction(new UniqueEventID() { Key = "abc", TimeStamp = DateTime.Now }, "{\"a\":1,\"_multi\":[{\"b\":2}]}");

                    // since there's not a model loaded why should get 100% exploration
                    Assert.AreEqual(1f, recorder.LastExplorerState.Probability);

                    model.Position = 0;
                    ds.UpdateModel(model);

                    decision = ds.ChooseAction(new UniqueEventID() { Key = "abc", TimeStamp = DateTime.Now }, "{\"a\":1,\"_multi\":[{\"b\":2}, {\"b\":3}]}");
                    Assert.AreNotEqual(1f, recorder.LastExplorerState.Probability);

                    var vwState = recorder.LastMapperState as VWState;
                    Assert.IsNotNull(vwState);
                    Assert.AreEqual("123", vwState.ModelId);
                }
            }
        }
    }
}
