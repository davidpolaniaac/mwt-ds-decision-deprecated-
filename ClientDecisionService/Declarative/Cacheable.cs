using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative
{
    [AttributeUsage(AttributeTargets.Property | AttributeTargets.Class)]
    public class Cacheable : Attribute
    {
        public Type EqualityComparer { get; set; }
    }
}
