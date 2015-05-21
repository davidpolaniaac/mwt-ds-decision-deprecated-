using MultiWorldTesting;
using System;
using System.Diagnostics;
using System.Linq;
using System.Globalization;
using Microsoft.Research.MachineLearning;

namespace ClientDecisionService
{
    /// <summary>
    /// Represent an updatable <see cref="IPolicy<TContext>"/> object which can consume different VowpalWabbit 
    /// models to predict a list of actions from an object of specified <see cref="TContext"/> type. This type 
    /// of object can also observe Azure Storage for newer model files.
    /// </summary>
    /// <typeparam name="TContext">The type of the context.</typeparam>
    internal class DecisionServicePolicy<TContext> : VWPolicy<TContext>
    {
        /// <summary>
        /// Constructor using the specified configurations.
        /// </summary>
        /// <param name="modelAddress">Uri address of the model blob to observe.</param>
        /// <param name="modelConnectionString">The connection string to access Azure Storage.</param>
        /// <param name="modelOutputDir">The output directory to download model blob to.</param>
        /// <param name="pollDelay">The polling delay which controls the frequency of checking for updates.</param>
        /// <param name="notifyPolicyUpdate">The callback to trigger when a new model is updated successfully.</param>
        /// <param name="modelPollFailureCallback">The callback to trigger when model polling fails.</param>
        public DecisionServicePolicy(string modelAddress, string modelConnectionString, 
            string modelOutputDir, TimeSpan pollDelay, 
            Action notifyPolicyUpdate, Action<Exception> modelPollFailureCallback)
        {
            if (pollDelay != TimeSpan.MinValue)
            {
                this.blobUpdater = new AzureBlobUpdater("model", modelAddress,
                   modelConnectionString, modelOutputDir, pollDelay,
                   this.UpdateFromFile, modelPollFailureCallback);
            }

            this.notifyPolicyUpdate = notifyPolicyUpdate;
        }

        /// <summary>
        /// Stop checking for new model update.
        /// </summary>
        public void StopPolling()
        {
            if (this.blobUpdater != null)
            {
                this.blobUpdater.StopPolling();
            }
        }

        /// <summary>
        /// Dispose the object.
        /// </summary>
        /// <param name="disposing">Whether the object is disposing resources.</param>
        protected override void Dispose(bool disposing)
        {
            base.Dispose(disposing);

            if (disposing)
            {
                // free managed resources
                if (this.blobUpdater != null)
                {
                    this.blobUpdater.Dispose();
                    this.blobUpdater = null;
                }
            }
        }

        /// <summary>
        /// Update new model from file and trigger callback if success.
        /// </summary>
        /// <param name="modelFile">The model file to load from.</param>
        /// <remarks>
        /// Triggered when a new model blob is found.
        /// </remarks>
        internal void UpdateFromFile(string modelFile)
        {
            bool modelUpdateSuccess = base.ModelUpdate(modelFile);

            if (modelUpdateSuccess && this.notifyPolicyUpdate != null)
            {
                this.notifyPolicyUpdate();
            }
        }

        AzureBlobUpdater blobUpdater;

        readonly Action notifyPolicyUpdate;
    }

}
