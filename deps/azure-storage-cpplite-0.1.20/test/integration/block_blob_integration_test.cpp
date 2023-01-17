#include "blob_integration_base.h"

#include "../catch2/catch.hpp"

TEST_CASE("Upload block blob from stream", "[block blob],[blob_service]")
{
    azure::storage_lite::blob_client client = as_test::base::test_blob_client();
    std::string container_name = as_test::create_random_container("", client);
    std::string blob_name = as_test::get_random_string(20);

    SECTION("Upload block blob from a 2048 byte stream successfully")
    {
        auto iss = as_test::get_istringstream_with_random_buffer(2048);
        auto create_blob_outcome = client.upload_block_blob_from_stream(container_name, blob_name, iss, std::vector<std::pair<std::string, std::string>>()).get();
        REQUIRE(create_blob_outcome.success());
        std::stringbuf strbuf;
        std::ostream os(&strbuf);
        auto get_blob_outcome = client.download_blob_to_stream(container_name, blob_name, 0, 4096, os).get();
        REQUIRE(get_blob_outcome.success());
        REQUIRE(strbuf.str() == iss.str());
    }

    SECTION("Upload block blob from a 64MB stream successfully")
    {
        auto iss = as_test::get_istringstream_with_random_buffer(64 * 1024 * 1024);
        auto create_blob_outcome = client.upload_block_blob_from_stream(container_name, blob_name, iss, std::vector<std::pair<std::string, std::string>>()).get();
        REQUIRE(create_blob_outcome.success());
        std::stringbuf strbuf;
        std::ostream os(&strbuf);
        auto get_blob_outcome = client.download_blob_to_stream(container_name, blob_name, 0, 64 * 1024 * 1024, os).get();
        REQUIRE(get_blob_outcome.success());
        REQUIRE(strbuf.str() == iss.str());
    }

    SECTION("Upload block blob from a 257MB stream unsuccessfully")
    {
        auto iss = as_test::get_istringstream_with_random_buffer(257 * 1024 * 1024);
        auto create_blob_outcome = client.upload_block_blob_from_stream(container_name, blob_name, iss, std::vector<std::pair<std::string, std::string>>()).get();
        REQUIRE(!create_blob_outcome.success());
    }

    SECTION("Upload block blob from with metadata successfully")
    {
        auto iss = as_test::get_istringstream_with_random_buffer(2048);
        std::vector<std::pair<std::string, std::string>> meta;
        meta.push_back(std::make_pair(std::string("custommeta1"), std::string("meta1")));
        meta.push_back(std::make_pair(std::string("custommeta2"), std::string("meta2")));
        auto create_blob_outcome = client.upload_block_blob_from_stream(container_name, blob_name, iss, meta).get();
        REQUIRE(create_blob_outcome.success());
        auto get_blob_property_outcome = client.get_blob_property(container_name, blob_name);
        REQUIRE(get_blob_property_outcome.success());
        for ( auto m : get_blob_property_outcome.response().metadata )
        {
            if (m.first == "custommeta1")
            {
                REQUIRE(m.second == "meta1");
            }
            else
            {
                REQUIRE(m.first == "custommeta2");
                REQUIRE(m.second == "meta2");

            }
        }
    }

    client.delete_container(container_name);
}

TEST_CASE("Upload blob block from stream", "[block blob],[blob_service]")
{
    azure::storage_lite::blob_client client = as_test::base::test_blob_client();
    std::string container_name = as_test::create_random_container("", client);
    std::string blob_name = as_test::get_random_string(20);

    SECTION("Upload block from stream successfully")
    {
        for (unsigned i = 0; i < 10; ++i)
        {
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            std::string block_id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, block_id, iss).get();
            REQUIRE(upload_block_outcome.success());
        }
    }

    SECTION("Upload block from buffer successfully")
    {
        size_t block_size = 4 * 1024 * 1024;
        unsigned block_count = 10;
        std::vector<azure::storage_lite::put_block_list_request_base::block_item> block_list;
        std::string full_content;
        for (unsigned i = 0; i < block_count; ++i)
        {
            azure::storage_lite::put_block_list_request_base::block_item item;
            auto buff = as_test::get_random_buffer(block_size);
            item.id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_buffer(container_name, blob_name, item.id, buff, block_size).get();
            REQUIRE(upload_block_outcome.success());
            item.type = azure::storage_lite::put_block_list_request_base::block_type::uncommitted;
            full_content.append(buff, block_size);
            block_list.push_back(item);
            free(buff);
        }

        auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
        REQUIRE(put_block_list_outcome.success());
        std::stringbuf strbuf;
        std::ostream os(&strbuf);
        auto get_blob_outcome = client.download_blob_to_stream(container_name, blob_name, 0, block_size * block_count, os).get();
        REQUIRE(get_blob_outcome.success());
        REQUIRE(strbuf.str() == full_content);
    }

    SECTION("Upload block from stream with invalid block ID unsuccessfully")
    {
        auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
        std::string block_id = "000001";
        auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, block_id, iss).get();
        REQUIRE(!upload_block_outcome.success());
    }
    
    client.delete_container(container_name);
}

TEST_CASE("Put block list", "[block blob],[blob_service]")
{
    azure::storage_lite::blob_client client = as_test::base::test_blob_client();
    std::string container_name = as_test::create_random_container("", client);
    std::string blob_name = as_test::get_random_string(20);

    SECTION("Put block list with all blocks uncommitted successfully")
    {
        std::vector<azure::storage_lite::put_block_list_request_base::block_item> block_list;
        for (unsigned i = 0; i < 10; ++i)
        {
            azure::storage_lite::put_block_list_request_base::block_item item;
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            item.id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, item.id, iss).get();
            REQUIRE(upload_block_outcome.success());
            item.type = azure::storage_lite::put_block_list_request_base::block_type::uncommitted;
            block_list.push_back(item);
        }
        
        auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
        REQUIRE(put_block_list_outcome.success());
        auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
        REQUIRE(get_block_list_outcome.success());
        auto committed_block_list = get_block_list_outcome.response().committed;
        auto uncommitted_block_list = get_block_list_outcome.response().uncommitted;
        REQUIRE(committed_block_list.size() == 10);
        REQUIRE(uncommitted_block_list.size() == 0);
        auto get_blob_property_outcome = client.get_blob_property(container_name, blob_name);
        REQUIRE(get_blob_property_outcome.success());
        REQUIRE(get_blob_property_outcome.response().size == 4 * 1024 * 1024 * 10);
    }

    SECTION("Put block list with all blocks committed successfully")
    {
        std::vector<azure::storage_lite::put_block_list_request_base::block_item> block_list;
        for (unsigned i = 0; i < 10; ++i)
        {
            azure::storage_lite::put_block_list_request_base::block_item item;
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            item.id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, item.id, iss).get();
            REQUIRE(upload_block_outcome.success());
            item.type = azure::storage_lite::put_block_list_request_base::block_type::uncommitted;
            block_list.push_back(item);
        }
        {
            auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
            REQUIRE(put_block_list_outcome.success());
            auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
            REQUIRE(get_block_list_outcome.success());
            auto committed_block_list = get_block_list_outcome.response().committed;
            auto uncommitted_block_list = get_block_list_outcome.response().uncommitted;
            REQUIRE(committed_block_list.size() == 10);
            REQUIRE(uncommitted_block_list.size() == 0);
            auto get_blob_property_outcome = client.get_blob_property(container_name, blob_name);
            REQUIRE(get_blob_property_outcome.success());
            REQUIRE(get_blob_property_outcome.response().size == 4 * 1024 * 1024 * 10);
        }
        for (unsigned i = 0; i < 10; ++i)
        {
            block_list[i].type = azure::storage_lite::put_block_list_request_base::block_type::committed;
        }
        {
            block_list.pop_back();
            auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
            REQUIRE(put_block_list_outcome.success());
            auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
            REQUIRE(get_block_list_outcome.success());
            auto committed_block_list = get_block_list_outcome.response().committed;
            auto uncommitted_block_list = get_block_list_outcome.response().uncommitted;
            REQUIRE(committed_block_list.size() == 9);
            REQUIRE(uncommitted_block_list.size() == 0);

            auto get_blob_property_outcome = client.get_blob_property(container_name, blob_name);
            REQUIRE(get_blob_property_outcome.success());
            REQUIRE(get_blob_property_outcome.response().size == 4 * 1024 * 1024 * 9);
        }
    }

    SECTION("Put block list with both committed and uncommitted blocks successfully")
    {
        std::vector<azure::storage_lite::put_block_list_request_base::block_item> block_list;
        for (unsigned i = 0; i < 5; ++i)
        {
            azure::storage_lite::put_block_list_request_base::block_item item;
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            item.id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, item.id, iss).get();
            REQUIRE(upload_block_outcome.success());
            item.type = azure::storage_lite::put_block_list_request_base::block_type::uncommitted;
            block_list.push_back(item);
        }
        {
            auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
            REQUIRE(put_block_list_outcome.success());
            auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
            REQUIRE(get_block_list_outcome.success());
            auto committed_block_list = get_block_list_outcome.response().committed;
            auto uncommitted_block_list = get_block_list_outcome.response().uncommitted;
            REQUIRE(committed_block_list.size() == 5);
            REQUIRE(uncommitted_block_list.size() == 0);
            auto get_blob_property_outcome = client.get_blob_property(container_name, blob_name);
            REQUIRE(get_blob_property_outcome.success());
            REQUIRE(get_blob_property_outcome.response().size == 4 * 1024 * 1024 * 5);
        }
        for (unsigned i = 0; i < 5; ++i)
        {
            block_list[i].type = azure::storage_lite::put_block_list_request_base::block_type::committed;
        }
        block_list.pop_back();
        for (unsigned i = 5; i < 10; ++i)
        {
            azure::storage_lite::put_block_list_request_base::block_item item;
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            item.id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, item.id, iss).get();
            REQUIRE(upload_block_outcome.success());
            item.type = azure::storage_lite::put_block_list_request_base::block_type::uncommitted;
            block_list.push_back(item);
        }
        {
            auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
            REQUIRE(put_block_list_outcome.success());
            auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
            REQUIRE(get_block_list_outcome.success());
            auto committed_block_list = get_block_list_outcome.response().committed;
            auto uncommitted_block_list = get_block_list_outcome.response().uncommitted;
            REQUIRE(committed_block_list.size() == 9);
            REQUIRE(uncommitted_block_list.size() == 0);

            auto get_blob_property_outcome = client.get_blob_property(container_name, blob_name);
            REQUIRE(get_blob_property_outcome.success());
            REQUIRE(get_blob_property_outcome.response().size == 4 * 1024 * 1024 * 9);
        }
    }

    SECTION("Put block list with invalid block list unsuccessfully")
    {
        std::vector<azure::storage_lite::put_block_list_request_base::block_item> block_list;
        for (unsigned i = 0; i < 10; ++i)
        {
            azure::storage_lite::put_block_list_request_base::block_item item;
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            item.id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, item.id, iss).get();
            REQUIRE(upload_block_outcome.success());
            item.type = azure::storage_lite::put_block_list_request_base::block_type::committed;
            block_list.push_back(item);
        }

        auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
        REQUIRE(!put_block_list_outcome.success());
    }

    client.delete_container(container_name);
}

TEST_CASE("Get block list", "[block blob],[blob_service]")
{
    azure::storage_lite::blob_client client = as_test::base::test_blob_client();
    std::string container_name = as_test::create_random_container("", client);
    std::string blob_name = as_test::get_random_string(20);

    SECTION("Get committed block list successfully")
    {
        std::vector<azure::storage_lite::put_block_list_request_base::block_item> block_list;
        for (unsigned i = 0; i < 10; ++i)
        {
            azure::storage_lite::put_block_list_request_base::block_item item;
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            item.id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, item.id, iss).get();
            REQUIRE(upload_block_outcome.success());
            item.type = azure::storage_lite::put_block_list_request_base::block_type::uncommitted;
            block_list.push_back(item);
        }

        auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
        REQUIRE(put_block_list_outcome.success());

        auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
        REQUIRE(get_block_list_outcome.success());
        auto committed_block_list = get_block_list_outcome.response().committed;
        auto uncommitted_block_list = get_block_list_outcome.response().uncommitted;
        REQUIRE(committed_block_list.size() == 10);
        REQUIRE(uncommitted_block_list.size() == 0);
    }

    SECTION("Get un-committed block list successfully")
    {
        for (unsigned i = 0; i < 10; ++i)
        {
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            std::string block_id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, block_id, iss).get();
            REQUIRE(upload_block_outcome.success());
        }

        auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
        REQUIRE(get_block_list_outcome.success());
        auto committed_block_list = get_block_list_outcome.response().committed;
        auto uncommitted_block_list = get_block_list_outcome.response().uncommitted;
        REQUIRE(committed_block_list.size() == 0);
        REQUIRE(uncommitted_block_list.size() == 10);
    }

    SECTION("Get both committed and uncommitted block list successfully")
    {
        std::vector<azure::storage_lite::put_block_list_request_base::block_item> block_list;
        for (unsigned i = 0; i < 10; ++i)
        {
            azure::storage_lite::put_block_list_request_base::block_item item;
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            item.id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, item.id, iss).get();
            REQUIRE(upload_block_outcome.success());
            item.type = azure::storage_lite::put_block_list_request_base::block_type::uncommitted;
            block_list.push_back(item);
        }

        auto put_block_list_outcome = client.put_block_list(container_name, blob_name, block_list, std::vector<std::pair<std::string, std::string>>()).get();
        REQUIRE(put_block_list_outcome.success());

        for (unsigned i = 0; i < 10; ++i)
        {
            auto iss = as_test::get_istringstream_with_random_buffer(4 * 1024 * 1024);
            auto block_id = as_test::get_base64_block_id(i);
            auto upload_block_outcome = client.upload_block_from_stream(container_name, blob_name, block_id, iss).get();
            REQUIRE(upload_block_outcome.success());
        }

        auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
        REQUIRE(get_block_list_outcome.success());
        auto committed_block_list = get_block_list_outcome.response().committed;
        auto uncommitted_block_list = get_block_list_outcome.response().uncommitted;
        REQUIRE(committed_block_list.size() == 10);
        REQUIRE(uncommitted_block_list.size() == 10);
    }

    SECTION("Get empty block list successfully")
    {
        auto get_block_list_outcome = client.get_block_list(container_name, blob_name).get();
        REQUIRE(!get_block_list_outcome.success());
        REQUIRE(get_block_list_outcome.error().code == "404");
        REQUIRE(get_block_list_outcome.error().code_name == "BlobNotFound");
    }

    client.delete_container(container_name);
}