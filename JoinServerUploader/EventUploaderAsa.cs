using Microsoft.Practices.EnterpriseLibrary.TransientFaultHandling;
using Microsoft.ServiceBus;
using Microsoft.ServiceBus.Messaging;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Reactive.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Threading.Tasks.Dataflow;
using System.Collections.Concurrent;

namespace Microsoft.Research.DecisionService.Uploader
{
    /// <summary>
    /// Uploader class to interface with the ASA-based Join Server provided by user applications.
    /// </summary>
    public class EventUploaderAsa : IDisposable
    {
        private string connectionString;
        private string eventHubInputName;
        private EventHubClient client;

        /// <summary>
        /// Constructs an uploader object.
        /// </summary>
        public EventUploaderAsa(string connectionString, string eventHubInputName)
        {
            this.connectionString = connectionString;
            this.eventHubInputName = eventHubInputName;
            
            var builder = new ServiceBusConnectionStringBuilder(this.connectionString)
            {
                TransportType = TransportType.Amqp
            };
            this.client = EventHubClient.CreateFromConnectionString(builder.ToString(), this.eventHubInputName);
        }

        public void Upload(IEvent e) 
        {
            this.UploadToEventHub(e);
        }

        public void UploadConcurrent(List<IEvent> events)
        {
            Parallel.For(
                fromInclusive: 0, 
                toExclusive: events.Count, 
                parallelOptions: new ParallelOptions { MaxDegreeOfParallelism = Environment.ProcessorCount * 2 }, 
                body: i => 
            {
                // Sending messages with EventHubClient is thread-safe (but not necessarily so for other APIs)
                // http://stackoverflow.com/questions/26898930/what-azure-net-sdk-eventhubclient-instance-methods-are-threadsafe
                this.UploadToEventHub(events[i]);
            });
        }

        public async Task UploadAsync(IEvent e)
        {
            await this.UploadToEventHubAsync(e);
        }

        public async Task UploadAsync(List<IEvent> events)
        {
            await Task.WhenAll(events.Select(e => this.UploadAsync(e)));
        }

        /// <summary>
        /// Flush the data buffer to upload all remaining events.
        /// </summary>
        public void Flush()
        {
        }

        /// <summary>
        /// Disposes the current object and all internal members.
        /// </summary>
        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        private void UploadToEventHub(IEvent e)
        {
            try
            {
                this.client.Send(BuildEventHubData(e));
            }
            catch (Exception exp)
            {
                Console.WriteLine("Error on send: " + exp.Message);
            }
        }

        private async Task UploadToEventHubAsync(IEvent e)
        {
            try
            {
                await this.client.SendAsync(BuildEventHubData(e));
            }
            catch (Exception exp)
            {
                Console.WriteLine("Error on send: " + exp.Message);
            }
        }

        private void Dispose(bool disposing)
        {
            if (disposing)
            {
            }
        }

        private static string BuildJsonMessage(IEvent e)
        {
            var jsonBuilder = new StringBuilder();
            jsonBuilder.Append("{\"EventId\":\"" + e.Key + "\",");
            jsonBuilder.Append("\"j\":[");
            jsonBuilder.Append(JsonConvert.SerializeObject(e));
            jsonBuilder.Append("]}");
            return jsonBuilder.ToString();
        }

        private static EventData BuildEventHubData(IEvent e)
        {
            var serializedString = BuildJsonMessage(e);
            return new EventData(Encoding.UTF8.GetBytes(serializedString))
            {
                PartitionKey = e.Key
            };
        }

        private void RaiseSentEvent(EventBatch batch)
        {
            if (batch != null)
            {
                if (batch.JsonEvents != null)
                {
                    Trace.TraceInformation("Successfully uploaded batch with {0} events.", batch.JsonEvents.Count);
                }
                if (PackageSent != null)
                {
                    PackageSent(this, new PackageEventArgs { PackageId = batch.Id, Records = batch.JsonEvents });
                }
            }
        }

        private void RaiseSendFailedEvent(EventBatch batch, Exception ex)
        {
            if (batch != null)
            {
                if (ex != null)
                {
                    Trace.TraceError("Unable to upload batch: " + ex.ToString());
                }
                if (PackageSendFailed != null)
                {
                    PackageSendFailed(this, new PackageEventArgs { PackageId = batch.Id, Records = batch.JsonEvents, Exception = ex });
                }
            }
        }

        /// <summary>
        /// Occurs when a package was successfully uploaded to the join server.
        /// </summary>
        public event PackageSentEventHandler PackageSent;

        /// <summary>
        /// Occurs when a package was not successfully uploaded to the join server.
        /// </summary>
        public event PackageSendFailedEventHandler PackageSendFailed;
    }
}
