using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative
{
    [AttributeUsage(AttributeTargets.Property)]
    public class FeatureAttribute : Attribute
    {
        public Type Converter { get; set; }

        public string Namespace { get; set; }

        public bool Enumerize { get; set; }

        internal char? InternalFeatureGroup { get; set; }

        public char FeatureGroup
        {
            get { return InternalFeatureGroup ?? ' '; }
            set { this.InternalFeatureGroup = value; }
        }
    }
}
