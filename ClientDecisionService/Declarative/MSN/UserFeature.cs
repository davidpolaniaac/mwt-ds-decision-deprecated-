using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.MSN
{
    public class UserFeature
    {
        // [Feature(Converter = typeof(AgeBinConverter))]
        [Feature]
        public Age Age { get; set; }

        [Feature(Enumerize = true)]
        public int? PassportAge { get; set; }

        [Feature]
        public Gender Gender { get; set; }

        [Feature]
        public string Location { get; set; }

        [Feature]
        public Dictionary<string, float> Provider { get; set; }

        [Feature]
        public Dictionary<int, float> CHE { get; set; }

        [Feature]
        public DayOfWeek DayOfWeek { get; set; }

        /// <summary>
        /// Will generate 24 hours
        /// </summary>
        [Feature(Enumerize =  true)]
        public int HourOfDay { get; set; }
    }



    public enum Age
    {
        O,
        P,
        Q,
        R,
        S
    }
}
