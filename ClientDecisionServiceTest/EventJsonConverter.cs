﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Microsoft.Research.MultiWorldTesting.JoinUploader;

namespace Microsoft.Research.MultiWorldTesting.ClientLibrary
{
    public class EventJsonConverter : JsonConverter
    {
        private static readonly string IEvent_Type = typeof(IEvent).FullName;

        public override bool CanConvert(Type objectType)
        {
            // COMMENT: why not objectType == typeof(IEvent) or typeof(IEvent).IsAssignableFrom(objectType)
            return objectType.FullName == IEvent_Type;
        }

        public override object ReadJson(JsonReader reader, Type objectType, object existingValue, JsonSerializer serializer)
        {
            if (objectType.FullName == IEvent_Type)
            {
                var jObject = JObject.Load(reader);
                switch ((EventType)Convert.ToInt32(jObject["t"]))
                {
                    case EventType.SingleActionInteraction:
                        return jObject.ToObject<SingleActionInteraction>();
                    case EventType.MultiActionInteraction:
                        return jObject.ToObject<MultiActionInteraction>();
                    case EventType.Observation:
                        return jObject.ToObject<Observation>();
                }
            }
            throw new NotSupportedException(string.Format("Type {0} unexpected.", objectType));
        }

        public override void WriteJson(Newtonsoft.Json.JsonWriter writer, object value, Newtonsoft.Json.JsonSerializer serializer)
        {
            serializer.Serialize(writer, value);
        }
    }
}
