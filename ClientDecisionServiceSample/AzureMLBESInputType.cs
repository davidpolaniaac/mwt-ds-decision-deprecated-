using Newtonsoft.Json;

namespace ClientDecisionServiceSample
{
    public class AzureMLBESInputType
    {
        public InputType Input { get; set; }
        public string Output { get; set; }
        public GlobalParametersType GlobalParameters { get; set; }

        public class InputType
        {
            public string ConnectionString { get; set; }
            public string RelativeLocation { get; set; }
            public string BaseLocation { get; set; }
            public string SasBlobToken { get; set; }
        }

        public class GlobalParametersType
        {
            [JsonProperty(PropertyName = "Authorization Token")]
            public string ReaderToken { get; set; }

            [JsonProperty(PropertyName = "Decision Service Authorization Token")]
            public string Token { get; set; }

            [JsonProperty(PropertyName = "Number of actions")]
            public int NumberOfActions { get; set; }
        }
    }
}
