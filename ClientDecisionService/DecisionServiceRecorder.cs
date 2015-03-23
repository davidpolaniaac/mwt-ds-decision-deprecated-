﻿using MultiWorldTesting;
using System;
using System.Linq;
using System.Collections.Generic;
using System.Reactive.Linq;
using System.Reactive.Subjects;
using Newtonsoft.Json;
using System.Net.Http;
using System.Net.Http.Headers;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Threading.Tasks.Dataflow;
using System.Diagnostics;

namespace ClientDecisionService
{
    // TODO: rename Recorder to Logger?
    internal class DecisionServiceRecorder<TContext> : IRecorder<TContext>, IDisposable
    {
        public DecisionServiceRecorder(BatchingConfiguration batchConfig, 
            Func<TContext, string> contextSerializer, 
            string authorizationToken) 
        {
            this.batchConfig = batchConfig;
            this.contextSerializer = contextSerializer;

            this.httpClient = new HttpClient();
            this.httpClient.BaseAddress = new Uri(this.ServiceAddress);
            this.httpClient.Timeout = this.ConnectionTimeOut;
            this.httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue(this.AuthenticationScheme, authorizationToken);

            this.eventSource = new TransformBlock<IEvent, string>(ev => JsonConvert.SerializeObject(new ExperimentalUnitFragment { Id = ev.ID, Value = ev }), 
                new ExecutionDataflowBlockOptions
            { 
                // TODO: Discuss whether we should expose another config setting for this BoundedCapacity
                MaxDegreeOfParallelism = Environment.ProcessorCount,
                BoundedCapacity = batchConfig.MaxUploadQueueCapacity
            });
            this.eventObserver = this.eventSource.AsObserver();

            this.eventProcessor = new ActionBlock<IList<string>>((Func<IList<string>, Task>)this.BatchProcess, new ExecutionDataflowBlockOptions 
            { 
                // TODO: Finetune these numbers
                MaxDegreeOfParallelism = Environment.ProcessorCount * 4,
                BoundedCapacity = batchConfig.MaxUploadQueueCapacity,
            });

            this.eventUnsubscriber = this.eventSource.AsObservable()
                .Window(batchConfig.MaxDuration)
                .Select(w => w.Buffer(batchConfig.MaxEventCount, batchConfig.MaxBufferSizeInBytes, json => Encoding.UTF8.GetByteCount(json)))
                .SelectMany(buffer => buffer)
                .Subscribe(this.eventProcessor.AsObserver());
        }

        // TODO: add a TryRecord that doesn't block and returns whether the operation was successful
        // TODO: alternatively we could also use a Configuration setting to control how Record() behaves
        public void Record(TContext context, uint action, float probability, string uniqueKey)
        {
            // Blocking call if queue is full.
            this.eventObserver.OnNext(new Interaction
            { 
                ID = uniqueKey,
                Action = (int)action,
                Probability = probability,
                Context = this.contextSerializer(context)
            });
        }

        public void ReportReward(float reward, string uniqueKey)
        {
            this.eventObserver.OnNext(new Observation
            {
                ID = uniqueKey,
                Value = JsonConvert.SerializeObject(new { Reward = reward })
            });
        }

        public bool TryReportReward(float reward, string uniqueKey)
        {
            return this.eventSource.Post(new Observation
            {
                ID = uniqueKey,
                Value = JsonConvert.SerializeObject(reward)
            });
        }

        public void ReportOutcome(string outcomeJson, string uniqueKey)
        {
            this.eventObserver.OnNext(new Observation
            { 
                ID = uniqueKey,
                Value = outcomeJson
            });
        }

        public bool TryReportOutcome(string outcomeJson, string uniqueKey)
        {
            return this.eventSource.Post(new Observation
            {
                ID = uniqueKey,
                Value = outcomeJson
            });
        }

        // TODO: at the time of server communication, if the client is out of memory (or meets some predefined upper bound):
        // 1. It can block the execution flow.
        // 2. Or drop events.
        private async Task BatchProcess(IList<string> jsonExpFragments)
        {
            EventBatch batch = new EventBatch { 
                ID = Guid.NewGuid(),
                JsonEvents = jsonExpFragments
            };

            byte[] jsonByteArray = Encoding.UTF8.GetBytes(this.BuildJsonMessage(batch));

            using (var jsonMemStream = new MemoryStream(jsonByteArray))
            {
#if TEST
                await this.BatchLog("decision_service_test_output", jsonMemStream);
#else
                HttpResponseMessage response = await httpClient.PostAsync(this.ServicePostAddress, new StreamContent(jsonMemStream)).ConfigureAwait(false);
                if (!response.IsSuccessStatusCode)
                {
                    string taskReadResponse = await response.Content.ReadAsStringAsync();

                    Trace.TraceError("Unable to upload batch: " + taskReadResponse);

                    if (this.batchConfig.UploadRetryPolicy == BatchUploadRetryPolicy.Retry)
                    {
                        // TODO: 2 options to handle retry:
                        // 1. Push events back to queue
                        // 2. Try to re-upload

                        // TODO: How long should we retry for? Configurable?
                        // TODO: throw exception with custom message if retry fails repeatedly?
                    }
                }
                else
                {
                    Trace.TraceInformation("Successfully uploaded batch with {0} events.", batch.JsonEvents.Count);
                }
#endif
            }
        }

        public void Flush()
        { 
            this.eventSource.Complete();
            this.eventProcessor.Completion.Wait();
        }

        private string BuildJsonMessage(EventBatch batch)
        {
            // TODO: use automatic serialization instead of building JSON manually
            // COMMENT: since the JsonEvents are already strings I'd say building JSON manually is the best way to go
            StringBuilder jsonBuilder = new StringBuilder();

            jsonBuilder.Append("{\"i\":\"" + batch.ID.ToString() + "\",");
            
            jsonBuilder.Append("\"j\":[");
            jsonBuilder.Append(String.Join(",", batch.JsonEvents));
            jsonBuilder.Append("]}");

            return jsonBuilder.ToString();
        }

        // Internally, background tasks can get back latest model version as a return value from the HTTP communication with Ingress worker

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
                if (this.httpClient != null)
                {
                    this.httpClient.Dispose();
                    this.httpClient = null;
                }

                if (this.eventUnsubscriber != null)
                {
                    this.eventUnsubscriber.Dispose();
                    this.eventUnsubscriber = null;
                }
            }
        }

#if TEST
        private async Task BatchLog(string batchFile, MemoryStream jsonMemStream)
        {
            // TODO: use other mechanisms to flush data than writing to disk
            File.WriteAllText(batchFile, Encoding.UTF8.GetString(jsonMemStream.ToArray()));
        }
#endif

        #region Members
        private readonly BatchingConfiguration batchConfig;
        private readonly Func<TContext, string> contextSerializer;
        private readonly TransformBlock<IEvent, string> eventSource;
        private readonly IObserver<IEvent> eventObserver;
        private readonly ActionBlock<IList<string>> eventProcessor;
        private IDisposable eventUnsubscriber;
        private HttpClient httpClient;
        #endregion

        #region Constants
        private readonly string ServiceAddress = "http://decisionservice.cloudapp.net";
        //private readonly string ServiceAddress = "http://localhost:1362";
        private readonly string ServicePostAddress = "/DecisionService.svc/PostExperimentalUnits";
        private readonly TimeSpan ConnectionTimeOut = TimeSpan.FromMinutes(5);
        private readonly string AuthenticationScheme = "Bearer";
        #endregion
    }
}
