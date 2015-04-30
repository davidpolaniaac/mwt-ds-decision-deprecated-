using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using ClientDecisionService.Declarative;
using ClientDecisionService.Declarative.MSN;
using Newtonsoft.Json;

namespace DeclarativeSample
{
    class Program
    {
        static void Main(string[] args)
        {
            var d1 = new DocumentFeature
            {
                Id = "d1",
                Time = new DateTime(2015, 1, 1),
                Value = new[] {1.0, 2.0, 3.0}
            };

            var context = new MWTContext
            {
                User = new UserFeature
                {
                    Age = 25,
                    Gender = Gender.Female
                },
                UserLDATopicPreference = new[] { 0.1, 0.2, 0.3 },
                Documents = new[]
                {
                    d1,
                    new DocumentFeature
                    {
                        Id = "d2",
                        Time = new DateTime(2015,1,1),
                        Value = new [] { 1.0, 2.0, 3.0 }
                    },
                    d1
                }
            };

            var json = JsonConvert.SerializeObject(context, Formatting.Indented);

            var vw = new VWSerializer().Serialize(context);

            Console.WriteLine(json);
            Console.WriteLine(vw);
            Console.ReadKey();
        }
    }
}
