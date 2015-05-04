using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace ClientDecisionService.Declarative.MSN
{
    public class LDAFeatureVector
    {
        public string Compressed { get; set; }

        private double[] values;

        [Feature]
        [JsonIgnore]
        public double[] Values
        {
            get
            {
                if (this.Compressed == null)
                {
                    // call into decompression
                }
                return this.values;
            }
            set { this.values = value; }
        }
    }
}
