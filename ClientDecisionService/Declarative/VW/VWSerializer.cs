using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace ClientDecisionService.Declarative.VW
{
    using VwHandle = IntPtr;

    public static class VWSerializer
    {
        public static IList<IntPtr> Serialize(VwHandle vw, object value)
        {
            return ExtractExample(value).ToVW(vw);
        }

        internal static Example ExtractExample(object value)
        {
            var props = value.GetType().GetProperties(BindingFlags.Instance | BindingFlags.GetProperty | BindingFlags.Public);

            var namespaces = ExtractFeatures(value, null, null)
            .GroupBy(f => new { f.Namespace, f.FeatureGroup, f.IsDense }, f => f)
            .Select(g => 
                // promote dense feature to namespace
                g.Key.IsDense ? 
                    (Namespace)new NamespaceDense
                    {
                        Name = g.Key.Namespace,
                        FeatureGroup = g.Key.FeatureGroup,
                        // TODO: check for multiple!
                        DenseFeature = g.First()
                    }
                    : new NamespaceSparse
                    {
                        Name = g.Key.Namespace,
                        FeatureGroup = g.Key.FeatureGroup,
                        Features = g.ToList()
                    });

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

        private static IList<Feature> ExtractFeatures(object value, string parentNamespace, char? parentFeatureGroup)
        {
            var props = value.GetType().GetProperties(BindingFlags.Instance | BindingFlags.GetProperty | BindingFlags.Public);

            var localFeatures = (from p in props
                                 let attr = p.GetCustomAttribute<FeatureAttribute>()
                                 where attr != null
                                 select new Feature
                                 {
                                     Namespace = attr.Namespace ?? parentNamespace,
                                     FeatureGroup = attr.InternalFeatureGroup ?? parentFeatureGroup,
                                     Source = value,
                                     Property = p,
                                     Converter = attr.Converter
                                 }).ToList();

            return localFeatures.Select(f => ExtractFeatures(f.Property.GetValue(value), f.Namespace, f.FeatureGroup))
                .SelectMany(f => f)
                .Union(localFeatures)
                .Where(f => f.IsConvertable)
                .ToList();
        }
    }
}
