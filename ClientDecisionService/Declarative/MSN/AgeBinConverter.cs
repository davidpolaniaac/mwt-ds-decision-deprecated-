using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.MSN
{
    public class AgeBinConverter : IVowpalWabbitFeatureConverter //<int>
    {
        public string Convert(PropertyInfo property, object obj)
        {
            var years = (int) obj;
            if (years < 10)
            {
                return "0-10";
            }
            else if (years < 30)
            {
                return "10-30";
            }

            return ">30";
        }
    }
}
