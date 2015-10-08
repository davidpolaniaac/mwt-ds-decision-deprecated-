﻿using Microsoft.Research.DecisionService.Uploader;
using MultiWorldTesting;
using Newtonsoft.Json;
using System;

namespace ClientDecisionService
{
    internal class JoinServiceLogger<TContext> : ILogger<TContext>, IDisposable
    {
        public void InitializeWithCustomAzureJoinServer(
            string authorizationToken,
            string loggingServiceBaseAddress,
            BatchingConfiguration batchConfig)
        {
            var eventUploader = new EventUploader(batchConfig, loggingServiceBaseAddress);
            eventUploader.InitializeWithToken(authorizationToken);
            
            this.eventUploader = eventUploader;
        }

        public void InitializeWithAzureStreamAnalyticsJoinServer(
            string eventHubConnectionString,
            string eventHubInputName,
            BatchingConfiguration batchConfig)
        {
            this.eventUploader = new EventUploaderASA(eventHubConnectionString, eventHubInputName, batchConfig);
        }

        public void Record(TContext context, uint[] actions, float probability, UniqueEventID uniqueKey)
        {
            this.eventUploader.TryUpload(new MultiActionInteraction
            { 
                Key = uniqueKey.Key,
                Id = uniqueKey.Id,
                TimeStamp = uniqueKey.TimeStamp,
                Actions = actions,
                Probability = probability,
                Context = context
            });
        }

        public void ReportReward(UniqueEventID uniqueKey, float reward)
        {
            this.eventUploader.TryUpload(new Observation
            {
                Key = uniqueKey.Key,
                Id = uniqueKey.Id,
                TimeStamp = uniqueKey.TimeStamp,
                Value = new { Reward = reward }
            });
        }

        public void ReportOutcome(UniqueEventID uniqueKey, object outcome)
        {
            this.eventUploader.TryUpload(new Observation
            {
                Key = uniqueKey.Key,
                Id = uniqueKey.Id,
                TimeStamp = uniqueKey.TimeStamp,
                Value = outcome
            });
        }

        public void Flush()
        {
            this.eventUploader.Flush();
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
                if (this.eventUploader != null)
                {
                    this.eventUploader.Dispose();
                    this.eventUploader = null;
                }
            }
        }

        #region Members
        private IEventUploader eventUploader;
        #endregion
    }
}
