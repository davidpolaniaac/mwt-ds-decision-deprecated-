using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.VW
{
    using VwHandle = IntPtr;

    internal abstract class Namespace
    {
        internal string Name { get; set; }

        internal char? FeatureGroup { get; set; }

        abstract internal void ToVW(VwHandle vw, VowpalWabbitInterface.FEATURE_SPACE featureSpace);

        public override string ToString()
        {
            return string.Format("|{0}{1} ",
                this.FeatureGroup,
                this.Name);
        }
    }
}
