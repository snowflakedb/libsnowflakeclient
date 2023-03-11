#include <string>
#include <chrono>
#include <thread>
#include <assert.h>
#include <fstream>
#include <sys/stat.h>

#include "storage_credential.h"
#include "storage_account.h"
#include "blob/blob_client.h"


using namespace azure::storage_lite;

void checkstatus()
{
    if(errno == 0)
    {
        printf("Success\n");
    }
    else
    {
        printf("Fail\n");
    }
}

int main()
{

    char caBundleFile[100]={0};
    strcpy(caBundleFile, "/etc/pki/tls/cacert.pem");

    std::ifstream infile; 
    infile.open("/home/vreddy/GetCredentials/credentials.conf");
    std::string account_name = "";
    infile >> account_name ;

    std::string endpoint = account_name + ".blob.core.windows.net";
    std::cout << "Endpoint: " << endpoint << std::endl;

    int parallel=10;

    std::string sas_key = "";
    infile >> sas_key ;
    std::cout << "SAS_KEY: " << sas_key << std::endl;
    std::shared_ptr<azure::storage_lite::storage_credential>  cred = std::make_shared<azure::storage_lite::shared_access_signature_credential>(sas_key);
    std::shared_ptr<azure::storage_lite::storage_account> account = std::make_shared<azure::storage_lite::storage_account>(account_name, cred, true, endpoint);

    auto bclient = std::make_shared<azure::storage_lite::blob_client>(account, parallel, caBundleFile);
    auto bc= new azure::storage_lite::blob_client_wrapper(bclient);

    std::string containerName = "";
    infile >> containerName ;
    std::cout << "Container Name: " << containerName << std::endl;

    std::string blobName = "uploadlog4.txt";
    std::string uploadFileName = "uploadlog4.txt";
    std::string downloadFileName = "downloadtext.log";

    std::cout << "blob exists" << std::endl;

    std::cout << "Start upload Blob: " << blobName << std::endl;

    std::vector<std::pair<std::string, std::string>> userMetadata;

    bc->upload_file_to_blob(blobName, containerName, blobName, userMetadata);

    if (errno == 0)
        std::cout << "Upload successful: " << std::endl;
    else
        std::cout << "error Upload FAILED: Errno " << errno << std::endl;

    if (bc->blob_exists(containerName,blobName))
    {
        std::cout << "blob exists" << std::endl ;
    }
    else{
        std::cout << "blob does not exists" << std::endl ;
    }

    auto blobProperty = bc->get_blob_property(containerName, blobName);
    if(errno == 0)
    {
        if(blobProperty.valid())
            std::cout <<"Size of Blob: " << blobProperty.size << std::endl;
        else
            std::cout <<"Invalid blob property" << std::endl ;
    }

    time_t last_modified;
    std::cout <<"Start blob download" << std::endl;
    bc->download_blob_to_file(containerName, blobName, downloadFileName, last_modified);
    if(errno == 0)
        std::cout <<"Download successful: " << std::endl;
    else
        std::cout <<"error download FAILED: Errno: " << errno << std::endl;

    std::cout <<"Delete Blob: " << blobName  << " in Container " << containerName << std::endl;
    bc->delete_blob(containerName, blobName);
    if(errno == 0)
        std::cout <<"Delete successful: " << std::endl;
    else
        std::cout <<"error Delete FAILED: " << std::endl;
    return 0;
}


/* 
void checkstatus()
{
    if(errno == 0)
    {
        printf("Success\n");
    }
    else
    {
        printf("Fail\n");
    }
}

int main()
{
    std::string account_name = "YOUR_ACCOUNT_NAME";
    std::string account_key = "YOUR_ACCOUNT_KEY";
    std::shared_ptr<storage_credential>  cred = std::make_shared<shared_key_credential>(account_name, account_key);
    std::shared_ptr<storage_account> account = std::make_shared<storage_account>(account_name, cred, false);
    auto bC = std::make_shared<blob_client>(account, 10);
    //auto f1 = bc.list_containers("");
    //f1.wait();
    //
    std::string containerName = "jasontest1";
    std::string blobName = "test.txt";
    std::string destContainerName = "jasontest1";
    std::string destBlobName = "test.txt.copy";
    std::string uploadFileName = "test.txt";
    std::string downloadFileName = "download.txt";

    bool exists = true;
    blob_client_wrapper bc(bC);
 
    exists = bc.container_exists(containerName);

    if(!exists)
    {
        bc.create_container(containerName);
        assert(errno == 0);
    }

    assert(errno == 0);
    exists = bc.blob_exists(containerName, "testsss.txt");
    assert(errno == 0);
    assert(!exists);
    std::cout <<"Start upload Blob: " << blobName << std::endl;
    bc.upload_file_to_blob(uploadFileName, containerName, blobName);
    std::cout <<"Error upload Blob: " << errno << std::endl;
    assert(errno == 0);

    exists = bc.blob_exists(containerName, blobName);
    assert(errno == 0);
    assert(exists);

    auto blobProperty = bc.get_blob_property(containerName, blobName);
    assert(errno == 0);
    std::cout <<"Size of BLob: " << blobProperty.size << std::endl;

    auto blobs = bc.list_blobs_segmented(containerName, "/", "", "");
    std::cout <<"Size of BLobs: " << blobs.blobs.size() << std::endl;
    std::cout <<"Error Size of BLobs: " << errno << std::endl;
    assert(errno == 0);

    time_t last_modified;
    bc.download_blob_to_file(containerName, blobName, downloadFileName, last_modified);
    std::cout <<"Download Blob done: " << errno << std::endl;
    assert(errno == 0);

    exists = bc.container_exists(destContainerName);

    if(!exists)
    {
        bc.create_container(destContainerName);
        assert(errno == 0);
    }

    // copy blob 
    bc.start_copy(containerName, blobName, destContainerName, destBlobName);
    auto property = bc.get_blob_property(destContainerName, destBlobName);
    std::cout << "Copy status: " << property.copy_status <<std::endl;
    exists = bc.blob_exists(destContainerName, destBlobName);
    assert(errno == 0);
    assert(exists);

    bc.delete_blob(containerName, blobName);
    bc.delete_blob(destContainerName, destBlobName);
    assert(errno == 0);
    exists = bc.blob_exists(containerName, blobName);
    assert(errno == 0);
    assert(!exists);
    //bc.delete_container(containerName);
    //assert(errno == 0);
    //std::this_thread::sleep_for(std::chrono::seconds(5));
}
*/