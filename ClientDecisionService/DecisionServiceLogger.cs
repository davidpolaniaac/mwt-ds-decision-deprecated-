using MultiWorldTesting;
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
using Microsoft.Practices.EnterpriseLibrary.TransientFaultHandling;
using System.Reflection;

namespace ClientDecisionService
{
    internal class DecisionServiceLogger<TContext> : ILogger<TContext>, IDisposable
        where TContext : IContext
    {
        public DecisionServiceLogger(BatchingConfiguration batchConfig, 
            Func<TContext, string> contextSerializer, 
            string authorizationToken,
            string loggingServiceBaseAddress) 
        {
            this.batchConfig = batchConfig ?? new BatchingConfiguration()
            {
                MaxBufferSizeInBytes = 4 * 1024 * 1024,
                MaxDuration = TimeSpan.FromMinutes(1),
                MaxEventCount = 10000,
                MaxUploadQueueCapacity = 100,
                UploadRetryPolicy = BatchUploadRetryPolicy.Retry
            };
            this.contextSerializer = contextSerializer ?? (x => x == null ? null : JsonConvert.SerializeObject(x));

            this.loggingServiceBaseAddress = loggingServiceBaseAddress ?? DecisionServiceConstants.ServiceAddress;

            this.httpClient = new HttpClient();
            this.httpClient.BaseAddress = new Uri(this.loggingServiceBaseAddress);
            this.httpClient.Timeout = DecisionServiceConstants.ConnectionTimeOut;
            this.httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue(DecisionServiceConstants.AuthenticationScheme, authorizationToken);

            // TODO: Switch to using latency-link upload strategy?
            this.eventSource = new TransformBlock<IEvent, string>(ev => JsonConvert.SerializeObject(new ExperimentalUnitFragment { Id = ev.ID, Value = ev }), 
                new ExecutionDataflowBlockOptions
            { 
                MaxDegreeOfParallelism = Environment.ProcessorCount,
                BoundedCapacity = this.batchConfig.MaxUploadQueueCapacity
            });
            this.eventObserver = this.eventSource.AsObserver();

            this.eventProcessor = new ActionBlock<IList<string>>((Func<IList<string>, Task>)this.BatchProcess, new ExecutionDataflowBlockOptions 
            { 
                // TODO: Finetune these numbers
                MaxDegreeOfParallelism = Environment.ProcessorCount * 4,
                BoundedCapacity = this.batchConfig.MaxUploadQueueCapacity,
            });

            this.eventUnsubscriber = this.eventSource.AsObservable()
                .Window(this.batchConfig.MaxDuration)
                .Select(w => w.Buffer(this.batchConfig.MaxEventCount, this.batchConfig.MaxBufferSizeInBytes, json => Encoding.UTF8.GetByteCount(json)))
                .SelectMany(buffer => buffer)
                .Subscribe(this.eventProcessor.AsObserver());

            this.asReferenceProperties =
            (
                from propInfo in typeof(TContext).GetProperties(BindingFlags.Public | BindingFlags.Instance)
                from arAttribute in propInfo.GetCustomAttributes<AsReferenceAttribute>()
                select new ContextProperties { PropInfo = propInfo, PropAttribute = arAttribute }
            ).ToList();

            // Compare using reference equality
            this.featureCache = new HashSet<object>(new ReferenceEqualityComparer());
        }

        // TODO: add a TryRecord that doesn't block and returns whether the operation was successful
        // TODO: alternatively we could also use a Configuration setting to control how Record() behaves
        public void Record(TContext context, uint action, float probability, string uniqueKey)
        {
            // Manually serialize features using compression scheme
            // Alternatively we could instantiate a new Context type with the proper Json.Net annotation and values
            var serializedCompressedContext = new StringBuilder();
            foreach (ContextProperties prop in this.asReferenceProperties)
            {
                object featureValue = prop.PropInfo.GetValue(context);

                var hasher = Activator.CreateInstance(prop.PropAttribute.Hasher) as IFeatureHasher;

                // For correctness use hash code if necessary
                //int featureHash = hasher.ComputeHash(featureValue);
                //if (this.featureCache.Contains(featureHash))

                // Otherwise check the actual object
                if (this.featureCache.Contains(featureValue))
                {
                    //serializedCompressedContext.Append(reference);
                }
                else
                {
                    //serializedCompressedContext.Append(value);
                }
            }

            // Cache global features
            object globalFeatures = context.GetGlobalFeatures();
            if (this.featureCache.Contains(globalFeatures))
            {
                //serializedCompressedContext.Append(reference);
            }
            else
            {
                //serializedCompressedContext.Append(value);
            }

            // Cache action features
            int numActions = context.GetNumberOfActions();
            for (int i = 0; i < numActions; i++)
            {
                object actionFeatures = context.GetActionFeatures(i);
                if (this.featureCache.Contains(actionFeatures))
                {
                    //serializedCompressedContext.Append(reference);
                }
                else
                {
                    //serializedCompressedContext.Append(value);
                }
            }

            // Blocking call if queue is full.
            this.eventObserver.OnNext(new Interaction
            { 
                ID = uniqueKey,
                Action = (int)action,
                Probability = probability,
                Context = serializedCompressedContext.ToString()
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

        public void ReportOutcome(string outcomeJson, string uniqueKey)
        {
            this.eventObserver.OnNext(new Observation
            { 
                ID = uniqueKey,
                Value = outcomeJson
            });
        }

        private async Task BatchProcess(IList<string> jsonExpFragments)
        {
            EventBatch batch = new EventBatch { 
                ID = Guid.NewGuid(),
                JsonEvents = jsonExpFragments
            };

            byte[] jsonByteArray = Encoding.UTF8.GetBytes(this.BuildJsonMessage(batch));

            using (var jsonMemStream = new MemoryStream(jsonByteArray))
            {
                HttpResponseMessage response = null;

                if (batchConfig.UploadRetryPolicy == BatchUploadRetryPolicy.Retry)
                {
                    var retryStrategy = new ExponentialBackoff(DecisionServiceConstants.RetryCount,
                    DecisionServiceConstants.RetryMinBackoff, DecisionServiceConstants.RetryMaxBackoff, DecisionServiceConstants.RetryDeltaBackoff);

                    RetryPolicy retryPolicy = new RetryPolicy<DecisionServiceTransientErrorDetectionStrategy>(retryStrategy);

                    response = await retryPolicy.ExecuteAsync(async () =>
                    {
                        HttpResponseMessage currentResponse = null;
                        try
                        {
                            currentResponse = await httpClient.PostAsync(DecisionServiceConstants.ServicePostAddress, new StreamContent(jsonMemStream)).ConfigureAwait(false);
                        }
                        catch (TaskCanceledException e) // HttpClient throws this on timeout
                        {
                            // Convert to a different exception otherwise ExecuteAsync will see cancellation
                            throw new HttpRequestException("Request timed out", e);
                        }
                        return currentResponse.EnsureSuccessStatusCode();
                    });
                }
                else
                {
                    response = await httpClient.PostAsync(DecisionServiceConstants.ServicePostAddress, new StreamContent(jsonMemStream)).ConfigureAwait(false);
                }
                
                if (!response.IsSuccessStatusCode)
                {
                    Trace.TraceError("Unable to upload batch: " + await response.Content.ReadAsStringAsync());
                }
                else
                {
                    Trace.TraceInformation("Successfully uploaded batch with {0} events.", batch.JsonEvents.Count);
                }
            }
        }

        public void Flush()
        { 
            this.eventSource.Complete();
            this.eventProcessor.Completion.Wait();
        }

        private string BuildJsonMessage(EventBatch batch)
        {
            StringBuilder jsonBuilder = new StringBuilder();

            jsonBuilder.Append("{\"i\":\"" + batch.ID.ToString() + "\",");
            
            jsonBuilder.Append("\"j\":[");
            jsonBuilder.Append(String.Join(",", batch.JsonEvents));
            jsonBuilder.Append("]}");

            return jsonBuilder.ToString();
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

        #region Members
        private readonly BatchingConfiguration batchConfig;
        private readonly Func<TContext, string> contextSerializer;
        private readonly TransformBlock<IEvent, string> eventSource;
        private readonly IObserver<IEvent> eventObserver;
        private readonly ActionBlock<IList<string>> eventProcessor;
        private readonly string loggingServiceBaseAddress;
        private IDisposable eventUnsubscriber;
        private HttpClient httpClient;

        private HashSet<object> featureCache;
        private List<ContextProperties> asReferenceProperties;
        #endregion
    }

    public class ContextProperties
    {
        public PropertyInfo PropInfo { get; set; }
        public AsReferenceAttribute PropAttribute { get; set; }
    }
}
