using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Research.DecisionService.Uploader
{
    public class StrictTypedEqualityComparer<T> : IEqualityComparer<object>
        where T : class
    {
        private IEqualityComparer<T> equalityComparer;

        public StrictTypedEqualityComparer(IEqualityComparer<T> equalityComparer)
        {
            this.equalityComparer = equalityComparer;
        }

        public bool Equals(object x, object y)
        {
            if (x == null && y == null)
            {
                return false;
            }

            var xt = x as T;
            var yt = y as T;

            if (xt == null || yt == null)
            {
                return false;
            }
               
            return this.equalityComparer.Equals(xt, yt);
        }

        public int GetHashCode(object obj)
        {
            var objt = obj as T;
            if (objt == null)
            {
                return -1;
            }

            return this.equalityComparer.GetHashCode(objt);
        }
    }
}
