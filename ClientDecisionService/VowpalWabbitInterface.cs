using System;
using System.Runtime.InteropServices;

namespace ClientDecisionService
{
    using SizeT = IntPtr;
    using VwExample = IntPtr;
    using VwHandle = IntPtr;
    using VwFeatureSpace = IntPtr;

    internal enum VowpalWabbitState
    {
        NotStarted = 0,
        Initialized,
        Finished
    }

    // COMMENT: why don't you model this as object wrapping VwHandle?
    internal sealed class VowpalWabbitInterface
    {
        private const string LIBVW = "Include\\libvw.dll";

        [StructLayout(LayoutKind.Sequential)]
        public struct FEATURE_SPACE
        {
            public byte name;
            public IntPtr features;     // points to a FEATURE[]
            public int len;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct FEATURE
        {
            public float x;
            public uint weight_index;
        }

        [DllImport(LIBVW, EntryPoint = "VW_Initialize")]
        public static extern VwHandle Initialize([MarshalAs(UnmanagedType.LPWStr)]string arguments);

        [DllImport(LIBVW, EntryPoint = "VW_Finish")]
        public static extern void Finish(VwHandle vw);

        [DllImport(LIBVW, EntryPoint = "VW_ReadExample")]
        public static extern VwExample ReadExample(VwHandle vw, [MarshalAs(UnmanagedType.LPWStr)]string exampleString);

        [DllImport(LIBVW, EntryPoint = "VW_FinishExample")]
        public static extern void FinishExample(VwHandle vw, VwExample example);

        [DllImport(LIBVW, EntryPoint = "VW_GetCostSensitivePrediction")]
        public static extern float GetCostSensitivePrediction(VwExample example);

        [DllImport(LIBVW, EntryPoint = "VW_Predict")]
        public static extern float Predict(VwHandle vw, VwExample example);

        [DllImport(LIBVW, EntryPoint = "VW_ImportExample")]
        // features points to a FEATURE_SPACE[]
        public static extern VwExample ImportExample(VwHandle vw, VwFeatureSpace features, SizeT length);

        // The DLL defines the last argument "u" as being an "unsigned long".
        // In C++ under current circumstances, both ints and longs are four byte integers.
        // If you wanted an eight byte integer you should use "long long" (or probably
        // more appropriately in this circumstance size_t).
        // In C#, "int" is four bytes, "long" is eight bytes.
        [DllImport(LIBVW, EntryPoint = "VW_HashFeature")]
        public static extern uint HashFeature(VwHandle vw, [MarshalAs(UnmanagedType.LPWStr)]string s, uint u);

        [DllImport(LIBVW, EntryPoint = "VW_HashSpace")]
        public static extern uint HashSpace(VwHandle vw, [MarshalAs(UnmanagedType.LPWStr)]string s);

        [DllImport(LIBVW, EntryPoint = "VW_AddLabel")]
        public static extern void AddLabel(VwExample example, float label = float.MaxValue, float weight = 1, float initial = 0);
    }

    internal sealed class VowpalWabbitInstance
    {
        internal VowpalWabbitInstance(string arguments)
        {
            vw = VowpalWabbitInterface.Initialize(arguments);
            vwState = VowpalWabbitState.Initialized;
        }

        internal uint Predict(string exampleLine)
        {
            IntPtr example = VowpalWabbitInterface.ReadExample(vw, exampleLine);
            VowpalWabbitInterface.Predict(vw, example);
            VowpalWabbitInterface.FinishExample(vw, example);
            return (uint)VowpalWabbitInterface.GetCostSensitivePrediction(example);
        }

        internal void Finish()
        {
            if (vwState == VowpalWabbitState.Initialized)
            {
                VowpalWabbitInterface.Finish(vw);
                vwState = VowpalWabbitState.Finished;
            }
        }

        IntPtr vw;
        VowpalWabbitState vwState;
    }
}
