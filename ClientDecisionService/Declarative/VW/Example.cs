using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.VW
{
    using VwHandle = IntPtr;

    internal class Example
    {
        internal string Comment { get; set; }

        internal List<Namespace> Namespaces { get; set; }

        internal List<Example> PerActionExamples { get; set; }

        public override string ToString()
        {
            var shared = string.Format("`{0} {1}",
                this.Comment,
                string.Join(" ", this.Namespaces));

            if (this.PerActionExamples == null)
            {
                return shared;
            }

            return string.Join("\n",
                new[] { shared }.Union(PerActionExamples.Select(p => p.ToString())));
        }

        internal IList<IntPtr> ToVW(VwHandle vw)
        {
            VowpalWabbitInterface.FEATURE_SPACE[] featureSpace = new VowpalWabbitInterface.FEATURE_SPACE[this.Namespaces.Count];

            for (int i = 0; i < this.Namespaces.Count; i++)
            {
                var ns = this.Namespaces[i];

                ns.ToVW(vw, featureSpace[i]);
            }

            GCHandle pinnedFeatureSpace = GCHandle.Alloc(featureSpace, GCHandleType.Pinned);

            // TODO: how to handle GCHandle (need to keep in memory?)
            // TODO: PerAction Features

            return null; // pinnedFeatureSpace.AddrOfPinnedObject();
        }
    }
}
