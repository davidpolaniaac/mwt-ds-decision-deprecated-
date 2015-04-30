using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative
{
    public class VWSerializer
    {
        public string Serialize(object value)
        {
            return ExtractExample(value).ToString();
        }

        private static Example ExtractExample(object value)
        {
            var props = value.GetType().GetProperties(BindingFlags.Instance | BindingFlags.GetProperty | BindingFlags.Public);

            var namespaces = ExtractFeatures(value, null)
            .GroupBy(f => f.Namespace, f => f)
            .Select(g => new Namespace { Name = g.Key, Features = g.ToList() });

            var example = new Example()
            {
                Namespaces = namespaces.ToList(),
                Comment = ExtractComment(value)
            };

            var perAction = (from p in props
                             let attr = p.GetCustomAttribute<PerActionFeaturesAttribute>()
                             where attr != null
                             select new { Property = p, Attribute = attr }
                ).FirstOrDefault();

            if (perAction != null)
            {
                // override VW keyword
                example.Comment = "shared";

                var perActionInstances = perAction.Property.GetValue(value) as IEnumerable<object>;

                if (perActionInstances == null)
                {
                    throw new InvalidOperationException("needs to be IEnumerable");
                }

                example.PerActionExamples = perActionInstances
                    .Select(ExtractExample)
                    .ToList();
            }

            return example;
        }

        private static string ExtractComment(object value)
        {
            var props = value.GetType().GetProperties(BindingFlags.Instance | BindingFlags.GetProperty | BindingFlags.Public);
            
            var comment = (from p in props
                let attr = p.GetCustomAttribute<CommentAttribute>()
                where attr != null
                select p)
                .FirstOrDefault();

            if (comment == null)
            {
                return null;
            }

            return comment.GetValue(value) as string;
        }

        private static IList<Feature> ExtractFeatures(object value, string parentNamespace)
        {
            var props = value.GetType().GetProperties(BindingFlags.Instance | BindingFlags.GetProperty | BindingFlags.Public);

            var localFeatures = (from p in props
                           let attr = p.GetCustomAttribute<FeatureAttribute>()
                           where attr != null
                           select new Feature
                            {
                                Namespace = attr.Namespace ?? parentNamespace,
                                Source = value,
                                Property = p,
                                Converter = attr.Converter
                           }).ToList();

            return localFeatures.Select(f => ExtractFeatures(f.Property.GetValue(value), f.Namespace))
                .SelectMany(f => f)
                .Union(localFeatures)
                .Where(f => f.IsConvertable)
                .ToList();
        }

        private class Namespace
        {
            internal string Name { get; set; }

            internal List<Feature> Features { get; set; }

            public override string ToString()
            {
                return string.Format("|{0} {1}",
                    this.Name,
                    string.Join(" ", Features.Select(f => f.ToString())));
            }
        }

        private class Example
        {
            internal string Comment { get; set; }

            internal List<Namespace> Namespaces { get; set; }

            internal List<Example> PerActionExamples { get; set; }

            public override string ToString()
            {
                var shared = string.Format("`{0} {1}",
                    this.Comment,
                    string.Join(" ", this.Namespaces));

                if (this.PerActionExamples == null)
                {
                    return shared;
                }

                return string.Join("\n",
                    new[] { shared }.Union(PerActionExamples.Select(p => p.ToString())));
            }
        }

        private class Feature
        {
            internal string Namespace { get; set; }

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

            public override string ToString()
            {
                var value = this.Property.GetValue(this.Source);

                if (this.Converter != null)
                {
                    var converter = Activator.CreateInstance(this.Converter) as IVowpalWabbitFeatureConverter;

                    // refine a bit to know if it's taking care of sparse with name or dense
                    return ":" + converter.Convert(this.Property, value);
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

                return string.Format("{0}:{1}", this.Property.Name, value);
            }
        }
    }
}
