﻿using Newtonsoft.Json;
using System;
using System.IO;
using System.Net;

namespace Microsoft.Research.MultiWorldTesting.Contract
{
    public static class ApplicationMetadataUtil
    {
        public static TMetadata DownloadMetadata<TMetadata>(string blobUri)
        {
            string jsonMetadata = "";
            try
            {
                using (var wc = new WebClient())
                {
                    jsonMetadata = wc.DownloadString(blobUri);
                    return JsonConvert.DeserializeObject<TMetadata>(jsonMetadata);
                }
            }
            catch (Exception ex)
            {
                throw new InvalidDataException("Unable to download metadata from specified blob uri " + blobUri + ", JSON: " + jsonMetadata, ex);
            }
        }
    }
}
