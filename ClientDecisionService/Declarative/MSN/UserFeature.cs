using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.MSN
{
    public class UserFeature
    {
        [Feature(Converter = typeof(AgeBinConverter))]
        public int Age { get; set; }

        [Feature]
        public Gender Gender { get; set; }
    }
}
