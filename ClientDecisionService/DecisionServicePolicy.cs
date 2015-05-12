﻿using MultiWorldTesting;
using System;
using System.Diagnostics;
using System.Globalization;

namespace ClientDecisionService
{
    internal class DecisionServicePolicy<TContext> : IPolicy<TContext>, IDisposable
    {
        public DecisionServicePolicy(string modelAddress, string modelConnectionString, 
            string modelOutputDir, TimeSpan pollDelay, 
            Action notifyPolicyUpdate, Action<Exception> modelPollFailureCallback)
        {
            if (pollDelay != TimeSpan.MinValue)
            {
                this.blobUpdater = new AzureBlobUpdater("model", modelAddress,
                   modelConnectionString, modelOutputDir, pollDelay,
                   this.ModelUpdate, modelPollFailureCallback);
            }

            this.notifyPolicyUpdate = notifyPolicyUpdate;
        }

        public uint[] ChooseAction(TContext context)
        {
            string exampleLine = string.Format(CultureInfo.InvariantCulture, "1: | {0}", context);

            if (this.vw == null)
            {
                throw new Exception("Internal Error: Vowpal Wabbit has not been initialized for scoring.");
            }

            return this.vw.Predict(exampleLine);
        }

        public void StopPolling()
        {
            if (this.blobUpdater != null)
            {
                this.blobUpdater.StopPolling();
            }

            if (this.vw != null)
            {
                this.vw.Finish();
            }
        }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
        {
            if (disposing)
            {
                // free managed resources
                if (this.blobUpdater != null)
                {
                    this.blobUpdater.Dispose();
                    this.blobUpdater = null;
                }
            }

            if (this.vw != null)
            {
                this.vw.Finish();
            }
        }

        void ModelUpdate(string modelFile)
        {
            bool modelUpdateSuccess = true;

            try
            {
                VowpalWabbitInstance oldVw = this.vw;
                this.vw = new VowpalWabbitInstance(string.Format(CultureInfo.InvariantCulture, "-t -i {0}", modelFile));

                if (oldVw != null)
                {
                    oldVw.Finish();
                }
            }
            catch (Exception ex)
            {
                Trace.TraceError("Unable to initialize VW.");
                Trace.TraceError(ex.ToString());
                modelUpdateSuccess = false;
            }

            if (modelUpdateSuccess)
            {
                this.notifyPolicyUpdate();
            }
            else
            {
                Trace.TraceInformation("Attempt to update model failed.");
            }
        }

        AzureBlobUpdater blobUpdater;

        VowpalWabbitInstance vw;

        readonly Action notifyPolicyUpdate;
    }

}
