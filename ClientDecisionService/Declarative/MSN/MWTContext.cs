using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace ClientDecisionService.Declarative.MSN
{
    public class MWTContext
    {
        [Feature(Namespace = "otheruser")]
        public UserFeature User { get; set; }

        [Feature(Namespace = "userlda")]
        public double[] UserLDATopicPreference { get; set; }

        [PerActionFeatures]
        [JsonProperty(ItemIsReference = true)]
        public IEnumerable<DocumentFeature> Documents { get; set; }
    }
}
