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

    internal class Feature
    {
        internal string Namespace { get; set; }

        internal char? FeatureGroup { get; set; }

        internal PropertyInfo Property { get; set; }

        internal object Source { get; set; }

        internal Type Converter { get; set; }

        internal bool IsConvertable
        {
            get
            {
                var propertyType = this.Property.PropertyType;

                return this.Converter != null ||
                       propertyType.IsEnum ||
                       propertyType == typeof(double) ||
                       propertyType == typeof(double[]);
            }
        }

        internal bool IsDense
        {
            get
            {
                // TODO: check if Converter targets numeric array output
                return this.Property.PropertyType == typeof (double[]) ||
                       this.Property.PropertyType == typeof (float[]);
            }
        }

        public override string ToString()
        {
            var value = this.Property.GetValue(this.Source);

            if (this.Converter != null)
            {
                var converter = Activator.CreateInstance(this.Converter) as IVowpalWabbitFeatureConverter;

                // refine a bit to know if it's taking care of sparse with name or dense
                return converter.Convert(this.Property, value);
            }

            if (this.Property.PropertyType.IsEnum)
            {
                return string.Format("{0}_{1}", this.Property.Name, Enum.GetName(this.Property.PropertyType, value));
            }


            var dblValues = value as double[];
            if (dblValues != null)
            {
                return string.Join(" ", dblValues.Select(v => ":" + v));
            }

            // TODO: more support for built-in types
            return string.Format("{0}:{1}", this.Property.Name, value);
        }

        internal void ToVW(VwHandle vw, VowpalWabbitInterface.FEATURE feature, uint namespaceHash)
        {
            var value = this.Property.GetValue(this.Source);

            if (this.Converter != null)
            {
                var converter = Activator.CreateInstance(this.Converter) as IVowpalWabbitFeatureConverter;

                // refine a bit to know if it's taking care of sparse with name or dense
                value = converter.Convert(this.Property, value);
            }
            else if (this.Property.PropertyType.IsEnum)
            {
                value = string.Format("{0}_{1}", this.Property.Name, Enum.GetName(this.Property.PropertyType, value));
            }

            var valueStr = value as string;
            if (valueStr != null)
            {
                // TODO: what's the reason for vw global data structure being passed
                feature.weight_index = VowpalWabbitInterface.HashFeature(vw, valueStr, namespaceHash);
                feature.x = 1;
            }

            var dblValue = value as double?;
            if (dblValue != null)
            {
                feature.weight_index = VowpalWabbitInterface.HashFeature(vw, this.Property.Name, namespaceHash);
                feature.x = (float)dblValue;
            }
        }
    }
}
