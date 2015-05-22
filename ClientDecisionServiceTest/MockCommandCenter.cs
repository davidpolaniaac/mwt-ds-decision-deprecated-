using Microsoft.Research.DecisionService.Common;
using Microsoft.Research.MachineLearning;
using Microsoft.WindowsAzure.Storage;
using Microsoft.WindowsAzure.Storage.Blob;
using Newtonsoft.Json;
using System;
using System.IO;
using System.Text;

namespace ClientDecisionServiceTest
{
    public class MockCommandCenter
    {
        public MockCommandCenter(string token)
        {
            this.token = token;
        }

        public void CreateBlobs(bool createSettingsBlob, bool createModelBlob, int modelId = 1)
        {
            if (createSettingsBlob || createModelBlob)
            {
                CloudStorageAccount storageAccount = CloudStorageAccount.Parse(MockCommandCenter.StorageConnectionString);
                CloudBlobClient blobClient = storageAccount.CreateCloudBlobClient();

                var localContainer = blobClient.GetContainerReference(this.localAzureContainerName);
                localContainer.CreateIfNotExists();

                if (createSettingsBlob)
                {
                    var settingsBlob = localContainer.GetBlockBlobReference(this.localAzureSettingsBlobName);
                    byte[] settingsContent = this.GetSettingsBlobContent();
                    settingsBlob.UploadFromByteArray(settingsContent, 0, settingsContent.Length);
                    this.localAzureSettingsBlobUri = settingsBlob.Uri.ToString();
                }

                if (createModelBlob)
                {
                    var modelBlob = localContainer.GetBlockBlobReference(this.localAzureModelBlobName);
                    byte[] modelContent = this.GetModelBlobContent(modelId);
                    modelBlob.UploadFromByteArray(modelContent, 0, modelContent.Length);
                    this.localAzureModelBlobUri = modelBlob.Uri.ToString();
                }

                var locationContainer = blobClient.GetContainerReference(this.localAzureBlobLocationContainerName);
                locationContainer.CreateIfNotExists();

                var publicAccessPermission = new BlobContainerPermissions()
                {
                    PublicAccess = BlobContainerPublicAccessType.Blob
                };

                locationContainer.SetPermissions(publicAccessPermission);

                var metadata = new ApplicationTransferMetadata
                {
                    ApplicationID = "test",
                    ConnectionString = MockCommandCenter.StorageConnectionString,
                    ExperimentalUnitDuration = 15,
                    IsExplorationEnabled = true,
                    ModelBlobUri = this.localAzureModelBlobUri,
                    SettingsBlobUri = this.localAzureSettingsBlobUri,
                    ModelId = "latest"
                };

                var locationBlob = locationContainer.GetBlockBlobReference(this.token);
                byte[] locationBlobContent = Encoding.UTF8.GetBytes(JsonConvert.SerializeObject(metadata));
                locationBlob.UploadFromByteArray(locationBlobContent, 0, locationBlobContent.Length);
            }
        }

        public byte[] GetSettingsBlobContent()
        {
            return new byte[3] { 1, 2, 3 };
        }

        public byte[] GetModelBlobContent(int modelId = 1)
        {
            string modelFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "TestData", "vw" + modelId + ".model");
            return File.ReadAllBytes(modelFile);
        }

        public byte[] GetModelBlobContent(int numExamples, int numFeatureVectors)
        {
            Random rg = new Random(numExamples + numFeatureVectors);

            string localOutputDir = "test";
            string vwFileName = Path.Combine(localOutputDir, string.Format("test_vw_{0}.model", numExamples));
            string vwArgs = string.Format("--quiet --noconstant -f {0}", vwFileName);

            using (var vw = new VowpalWabbit<TestADFContextWithFeatures>(vwArgs))
            {
                //Create examples
                for (int ie = 0; ie < numExamples; ie++)
                {
                    // Create features
                    var context = TestADFContextWithFeatures.CreateRandom(numFeatureVectors, rg);

                    using (var example = vw.ReadExample(context))
                    {
                        example.Learn();
                    }
                }

                vw.SaveModel();
            }

            return File.ReadAllBytes(vwFileName);
        }

        public string LocalAzureSettingsBlobName
        {
            get { return localAzureSettingsBlobName; }
        }

        public string LocalAzureModelBlobName
        {
            get { return localAzureModelBlobName; }
        }

        private string token;
        private string localAzureSettingsBlobUri;
        private string localAzureModelBlobUri;

        private readonly string localAzureBlobLocationContainerName = "app-locations";
        private readonly string localAzureContainerName = "localtestcontainer";
        private readonly string localAzureSettingsBlobName = "localtestsettingsblob";
        private readonly string localAzureModelBlobName = "localtestmodelblob";

        public static readonly string StorageConnectionString = "UseDevelopmentStorage=true";
        public static readonly string AuthorizationToken = "test token";
    }
}
