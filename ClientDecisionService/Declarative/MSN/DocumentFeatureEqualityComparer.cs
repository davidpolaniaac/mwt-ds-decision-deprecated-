using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.MSN
{
    public class DocumentFeatureEqualityComparer : IEqualityComparer<DocumentFeature>
    {
        public bool Equals(DocumentFeature x, DocumentFeature y)
        {
            return x.Id == y.Id &&
                   x.Time == y.Time;
                // maybe compare the full vector - not so sure on this part though
                //   x.Value.Zip(y.Value, (a, b) => a == b).All(c => c);
        }

        public int GetHashCode(DocumentFeature obj)
        {
            return obj.Id.GetHashCode() + obj.Time.GetHashCode();
        }
    }
}
