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
    }
}
