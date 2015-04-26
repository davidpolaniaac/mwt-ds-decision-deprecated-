using MultiWorldTesting;
using System;
using System.Globalization;

namespace ClientDecisionService
{
    public class VWPolicy<TContext> : IPolicy<TContext>, IDisposable
        where TContext : IContext
    {
        public VWPolicy(string vwModelFile)
        {
            vw = new VowpalWabbitInstance(string.Format(CultureInfo.InvariantCulture, "-t -i {0}", vwModelFile));
        }

        public uint ChooseAction(TContext context, uint numActions)
        {
            return vw.Predict(string.Format(CultureInfo.InvariantCulture, "1: | {0}", context.ToVWString()));
        }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
        {
            if (disposing)
            {
                // free managed resources
            }
            vw.Finish();
        }

        VowpalWabbitInstance vw;
    }
}
