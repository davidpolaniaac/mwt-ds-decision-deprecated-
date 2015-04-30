using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace ClientDecisionService.Declarative.MSN
{
    [Cacheable(EqualityComparer = typeof(DocumentFeatureEqualityComparer))]
    public class DocumentFeature
    {
        [Comment]
        public string Id { get; set; }

        [JsonIgnore]
        public DateTime Time { get; set; }

        // If we include this, it would result in mixing dense and non-dense features.
        // [Feature]
        public string ContentProvider { get; set; }

        [Feature(Namespace = "doclda")]
        public double[] Value { get; set; }
    }
}
