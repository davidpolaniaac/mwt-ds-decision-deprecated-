using Microsoft.Research.MachineLearning;
using MultiWorldTesting;
using System;
using System.Diagnostics;
using System.Linq;

namespace ClientDecisionService
{
    /// <summary>
    /// Represent an updatable <see cref="IPolicy<TContext>"/> object which can consume different VowpalWabbit 
    /// models to predict a list of actions from an object of specified <see cref="TContext"/> type.
    /// </summary>
    /// <typeparam name="TContext">The type of the context.</typeparam>
    public class VWPolicy<TContext> : IPolicy<TContext>, IDisposable
    {
        /// <summary>
        /// Constructor using an optional model file.
        /// </summary>
        /// <param name="vwModelFile">Optional; the VowpalWabbit model file to load from.</param>
        public VWPolicy(string vwModelFile = null)
        {
            if (vwModelFile == null)
            {
                this.vwPool = new ObjectPool<VowpalWabbit<TContext>>(() =>
                {
                    throw new InvalidOperationException("Internal Error: Vowpal Wabbit has not been initialized for scoring.");
                });
            }
            else
            {
                this.ModelUpdate(vwModelFile);
            }
        }

        /// <summary>
        /// Scores the model against the specified context and returns a list of actions (1-based index).
        /// </summary>
        /// <param name="context">The context object.</param>
        /// <returns>List of predicted actions.</returns>
        public uint[] ChooseAction(TContext context)
        {
            using (var vw = vwPool.Get())
            using (var example = vw.Value.ReadExample(context))
            {
                example.Predict();

                // VW multi-label predictions are 0-based
                return example.MultilabelPredictions.Select(p => (uint)(p + 1)).ToArray();
            }
        }

        /// <summary>
        /// Update VW model from file.
        /// </summary>
        /// <param name="modelFile">The model file to load.</param>
        /// <returns>true if the update was successful; otherwise, false.</returns>
        public bool ModelUpdate(string modelFile)
        {
            VowpalWabbitModel vwModel = null;
            try
            {
                // TODO: add Dispose to ObjectPool using reference couting to dispose the shared model correctly.
                // otherwise this is wasting memory as the shared model is never freed.
                vwModel = new VowpalWabbitModel(modelFile);
            }
            catch (Exception ex)
            {
                Trace.TraceError("Unable to initialize VW from file: {0}", modelFile);
                Trace.TraceError(ex.ToString());

                return false;
            }

            if (this.vwPool == null)
            {
                this.vwPool = new ObjectPool<VowpalWabbit<TContext>>(() => CreateNewVW(vwModel));
            }
            else
            {
                vwPool.UpdateFactory(() => CreateNewVW(vwModel));
            }
            return true;
        }

        /// <summary>
        /// Dispose the object and clean up any resources.
        /// </summary>
        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Dispose the object.
        /// </summary>
        /// <param name="disposing">Whether the object is disposing resources.</param>
        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (this.vwPool != null)
                {
                    this.vwPool.Dispose();
                    this.vwPool = null;
                }
            }
        }

        private VowpalWabbit<TContext> CreateNewVW(VowpalWabbitModel model)
        {
            return new VowpalWabbit<TContext>(model);
        }

        private ObjectPool<VowpalWabbit<TContext>> vwPool;
    }
}
