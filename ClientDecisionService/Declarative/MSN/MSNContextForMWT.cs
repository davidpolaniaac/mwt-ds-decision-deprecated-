using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace ClientDecisionService.Declarative.MSN
{
    public class MSNContextForMWT
    {
        [Feature(Namespace = "otheruser", FeatureGroup = 'o')]
        public UserFeature User { get; set; }

        [Feature(Namespace = "userlda", FeatureGroup = 'u')]
        public LDAFeatureVector UserLDATopicPreference { get; set; }

        [PerActionFeatures]
        [JsonProperty(ItemIsReference = true)]
        public IEnumerable<DocumentFeature> Documents { get; set; }
    }
}
