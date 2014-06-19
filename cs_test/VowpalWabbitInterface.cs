﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace Microsoft.Research.MachineLearning
{
    public sealed class VowpalWabbitInterface
    {
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

        [DllImport("libvw.dll", EntryPoint = "VW_Initialize", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr Initialize([MarshalAs(UnmanagedType.LPWStr)]string arguments);

        [DllImport("libvw.dll", EntryPoint = "VW_Finish", CallingConvention = CallingConvention.StdCall)]
        public static extern void Finish(IntPtr vw);
        
        [DllImport("libvw.dll", EntryPoint = "VW_ImportExample", CallingConvention = CallingConvention.StdCall)]
        // features points to a FEATURE_SPACE[]
        public static extern IntPtr ImportExample(IntPtr vw, IntPtr features, int length);

        [DllImport("libvw.dll", EntryPoint = "VW_ExportExample", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ExportExample(IntPtr vw, IntPtr example, ref int length);

        [DllImport("libvw.dll", EntryPoint = "VW_ReleaseFeatureSpace", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ReleaseFeatureSpace(IntPtr fs, int length);
       
        [DllImport("libvw.dll", EntryPoint = "VW_ReadExample", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ReadExample(IntPtr vw, [MarshalAs(UnmanagedType.LPWStr)]string exampleString);

        [DllImport("libvw.dll", EntryPoint = "VW_StartParser", CallingConvention = CallingConvention.StdCall)]
        public static extern void StartParser(IntPtr vw, bool do_init);

        [DllImport("libvw.dll", EntryPoint = "VW_EndParser", CallingConvention = CallingConvention.StdCall)]
        public static extern void EndParser(IntPtr vw);

        [DllImport("libvw.dll", EntryPoint = "VW_GetExample", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr GetExample(IntPtr parser);

        [DllImport("libvw.dll", EntryPoint = "VW_FinishExample", CallingConvention = CallingConvention.StdCall)]
        public static extern void FinishExample(IntPtr vw, IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_GetLabel", CallingConvention = CallingConvention.StdCall)]
        public static extern float GetLabel(IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_GetImportance", CallingConvention = CallingConvention.StdCall)]
        public static extern float GetImportance(IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_GetInitial", CallingConvention = CallingConvention.StdCall)]
        public static extern float GetInitial(IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_GetPrediction", CallingConvention = CallingConvention.StdCall)]
        public static extern float GetPrediction(IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_GetTagLength", CallingConvention = CallingConvention.StdCall)]
        public static extern UInt32 GetTagLength(IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_GetTag", CallingConvention = CallingConvention.StdCall)]
        public static extern byte GetTag(IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_GetFeatureNumber", CallingConvention = CallingConvention.StdCall)]
        public static extern UInt32 GetFeatureNumber(IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_GetFeatures", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr GetFeatures(IntPtr vw, IntPtr example, ref int length);

        [DllImport("libvw.dll", EntryPoint = "VW_ReturnFeatures", CallingConvention = CallingConvention.StdCall)]
        public static extern void ReturnFeatures(IntPtr features);

        [DllImport("libvw.dll", EntryPoint = "VW_HashSpace", CallingConvention = CallingConvention.StdCall)]
        public static extern uint HashSpace(IntPtr vw, [MarshalAs(UnmanagedType.LPWStr)]string s);
        
        [DllImport("libvw.dll", EntryPoint = "VW_HashFeature", CallingConvention = CallingConvention.StdCall)]
        public static extern uint HashFeature(IntPtr vw, [MarshalAs(UnmanagedType.LPWStr)]string s, ulong u);
        
        [DllImport("libvw.dll", EntryPoint = "VW_Learn", CallingConvention = CallingConvention.StdCall)]
        public static extern float Learn(IntPtr vw, IntPtr example);

        [DllImport("libvw.dll", EntryPoint = "VW_AddLabel", CallingConvention = CallingConvention.StdCall)]
        public static extern void AddLabel(IntPtr example, float label=float.MaxValue, float weight=1, float initial=0);

        [DllImport("libvw.dll", EntryPoint = "VW_Get_Weight", CallingConvention = CallingConvention.StdCall)]
        public static extern float Get_Weight(IntPtr vw, UInt32 index, UInt32 offset);

        [DllImport("libvw.dll", EntryPoint = "VW_Set_Weight", CallingConvention = CallingConvention.StdCall)]
        public static extern void Set_Weight(IntPtr vw, UInt32 index, UInt32 offset, float value);

        [DllImport("libvw.dll", EntryPoint = "VW_Num_Weights", CallingConvention = CallingConvention.StdCall)]
        public static extern UInt32 Num_Weights(IntPtr vw);

        [DllImport("libvw.dll", EntryPoint = "VW_Get_Stride", CallingConvention = CallingConvention.StdCall)]
        public static extern UInt32 Get_Stride(IntPtr vw);    
    }
}
