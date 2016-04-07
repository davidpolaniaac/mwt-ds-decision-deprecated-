﻿using Microsoft.Research.MultiWorldTesting.ExploreLibrary;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using VW;

namespace Microsoft.Research.MultiWorldTesting.ClientLibrary.VW
{
    internal class VWRanker<TContext>
        : VWBaseContextMapper<VowpalWabbitThreadedPrediction<TContext>, VowpalWabbit<TContext>, TContext, uint[]>, IRanker<TContext>
    {
        /// <summary>
        /// Constructor using a memory stream.
        /// </summary>
        /// <param name="vwModelStream">The VW model memory stream.</param>
        internal VWRanker(Stream vwModelStream = null, VowpalWabbitFeatureDiscovery featureDiscovery = VowpalWabbitFeatureDiscovery.Json)
            : base(vwModelStream, featureDiscovery)
        {
        }

        protected override Decision<uint[]> MapContext(VowpalWabbit<TContext> vw, TContext context)
        {
            int[] vwMultilabelPredictions = vw.Predict(context, VowpalWabbitPredictionType.Multilabel);

            // VW multi-label predictions are 0-based
            var actions = vwMultilabelPredictions.Select(a => (uint)(a + 1)).ToArray();
            var state = new VWState { ModelId = vw.Native.ID };

            return Decision.Create(actions, state);
        }
    }

    internal class VWRanker<TContext, TActionDependentFeature>
        : VWBaseContextMapper<VowpalWabbitThreadedPrediction<TContext, TActionDependentFeature>, VowpalWabbit<TContext, TActionDependentFeature>, TContext, uint[]>, IRanker<TContext>
    {
        private readonly Func<TContext, IReadOnlyCollection<TActionDependentFeature>> getContextFeaturesFunc;

        /// <summary>
        /// Constructor using a memory stream.
        /// </summary>
        /// <param name="vwModelStream">The VW model memory stream.</param>
        internal VWRanker(
            Func<TContext, IReadOnlyCollection<TActionDependentFeature>> getContextFeaturesFunc,
            Stream vwModelStream = null,
            VowpalWabbitFeatureDiscovery featureDiscovery = VowpalWabbitFeatureDiscovery.Default)
            : base(vwModelStream, featureDiscovery)
        {
            this.getContextFeaturesFunc = getContextFeaturesFunc;
        }

        protected override Decision<uint[]> MapContext(VowpalWabbit<TContext, TActionDependentFeature> vw, TContext context)
        {
            IReadOnlyCollection<TActionDependentFeature> features = this.getContextFeaturesFunc(context);

            // return indices
            ActionDependentFeature<TActionDependentFeature>[] vwMultilabelPredictions = vw.Predict(context, features);

            // VW multi-label predictions are 0-based
            var actions = vwMultilabelPredictions.Select(p => (uint)(p.Index + 1)).ToArray();
            var state = new VWState { ModelId = vw.Native.ID };

            return Decision.Create(actions, state);
        }
    }

}
