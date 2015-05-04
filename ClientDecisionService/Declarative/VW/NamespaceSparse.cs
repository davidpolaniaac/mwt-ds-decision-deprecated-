using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.VW
{
    using VwHandle = IntPtr;

    internal class NamespaceSparse : Namespace
    {
        internal List<Feature> Features { get; set; }

        public override string ToString()
        {
            return base.ToString() + string.Join(" ", Features.Select(f => f.ToString()));
        }

        override internal void ToVW(VwHandle vw, VowpalWabbitInterface.FEATURE_SPACE featureSpace)
        {
            var features = new VowpalWabbitInterface.FEATURE[this.Features.Count];
            var pinnedsFeatures = GCHandle.Alloc(features, GCHandleType.Pinned);

            featureSpace.name = (byte)this.FeatureGroup;
            featureSpace.features = pinnedsFeatures.AddrOfPinnedObject();
            featureSpace.len = this.Features.Count;

            var namespaceHash = this.Name == null ? 0 : VowpalWabbitInterface.HashSpace(vw, this.Name);
            for (var i = 0; i < this.Features.Count; i++)
            {
                this.Features[i].ToVW(vw, features[i], namespaceHash);
            }
        }
    }
}
