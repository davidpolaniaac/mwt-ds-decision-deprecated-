using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.VW
{
    public static class VWStringSerializer
    {
        public static string Serialize(object value)
        {
            return VWSerializer.ExtractExample(value).ToString();
        }
    }
}
