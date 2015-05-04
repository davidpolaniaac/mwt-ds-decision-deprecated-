using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.VW
{
    using VwHandle = IntPtr;

    internal class NamespaceDense : Namespace
    {
        internal Feature DenseFeature { get; set; }

        public override string ToString()
        {
            return base.ToString() + this.DenseFeature.ToString();
        }

        override internal void ToVW(VwHandle vw, VowpalWabbitInterface.FEATURE_SPACE featureSpace)
        {
            var value = this.DenseFeature.Property.GetValue(this.DenseFeature.Source);

            if (this.DenseFeature.Converter != null)
            {
                var converter = Activator.CreateInstance(this.DenseFeature.Converter) as IVowpalWabbitFeatureConverter;

                // refine a bit to know if it's taking care of sparse with name or dense
                value = converter.Convert(this.DenseFeature.Property, value);
            }
            
            var dblValues = (double[])value;

            var features = new VowpalWabbitInterface.FEATURE[dblValues.Length];
            var pinnedsFeatures = GCHandle.Alloc(features, GCHandleType.Pinned);

            featureSpace.name = (byte)this.FeatureGroup;
            featureSpace.features = pinnedsFeatures.AddrOfPinnedObject();
            featureSpace.len = dblValues.Length;

            // offset the feature index
            var namespaceHash = this.Name == null ? 0 : VowpalWabbitInterface.HashSpace(vw, this.Name);

            for (var i = 0; i < dblValues.Length; i++)
            {
                features[i].weight_index = (uint)(namespaceHash + i);
                features[i].x = (float)dblValues[i];
            }
        }
    }
}
