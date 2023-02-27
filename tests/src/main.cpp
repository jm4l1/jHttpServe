#define CATCH_CONFIG_MAIN

#include "../lib/catch.hpp"
#include "HPack.h"
#include "Huffman.h"

#include <vector>

TEST_CASE("Huffman encoding a string")
{
	{
		const std::string given_value("DELETE");
		std::vector<uint8_t> expected_encoded_buffer = { 0xbf, 0x83, 0x3e, 0x0d, 0xf8, 0x3f };
		auto encoded_buffer = Huffman::Encode(given_value);
		REQUIRE(expected_encoded_buffer == encoded_buffer);
	}
	{
		const std::string given_value("www.example.com");
		std::vector<uint8_t> expected_encoded_buffer = { 0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff };
		auto encoded_buffer = Huffman::Encode(given_value);
		REQUIRE(expected_encoded_buffer == encoded_buffer);
	}
	{
		const std::string given_value("no-cache");
		std::vector<uint8_t> expected_encoded_buffer = { 0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf };
		auto encoded_buffer = Huffman::Encode(given_value);
		REQUIRE(expected_encoded_buffer == encoded_buffer);
	}
	{
		const std::string given_value("custom-key");
		std::vector<uint8_t> expected_encoded_buffer = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f };
		auto encoded_buffer = Huffman::Encode(given_value);
		REQUIRE(expected_encoded_buffer == encoded_buffer);
	}
	{
		const std::string given_value("custom-value");
		std::vector<uint8_t> expected_encoded_buffer = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf };
		auto encoded_buffer = Huffman::Encode(given_value);
		REQUIRE(expected_encoded_buffer == encoded_buffer);
	}
}
TEST_CASE("Decoding Huffman encoded buffer")
{
	{
		const std::string expected_string("www.example.com");
		std::vector<uint8_t> encoded_buffer = { 0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff };
		auto decoder_buffer = Huffman::Decode(encoded_buffer);
		REQUIRE(expected_string == std::string(decoder_buffer.begin(), decoder_buffer.end()));
	}
	{
		const std::string expected_string("DELETE");
		std::vector<uint8_t> encoded_buffer = { 0xbf, 0x83, 0x3e, 0x0d, 0xf8, 0x3f };
		auto decoder_buffer = Huffman::Decode(encoded_buffer);
		REQUIRE(expected_string == std::string(decoder_buffer.begin(), decoder_buffer.end()));
	}
	{
		const std::string expected_string("no-cache");
		std::vector<uint8_t> encoded_buffer = { 0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf };
		auto decoder_buffer = Huffman::Decode(encoded_buffer);
		REQUIRE(expected_string == std::string(decoder_buffer.begin(), decoder_buffer.end()));
	}
	{
		const std::string expected_string("custom-key");
		std::vector<uint8_t> encoded_buffer = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f };
		auto decoder_buffer = Huffman::Decode(encoded_buffer);
		REQUIRE(expected_string == std::string(decoder_buffer.begin(), decoder_buffer.end()));
	}
	{
		const std::string expected_string("custom-value");
		std::vector<uint8_t> encoded_buffer = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf };
		auto decoder_buffer = Huffman::Decode(encoded_buffer);
		REQUIRE(expected_string == std::string(decoder_buffer.begin(), decoder_buffer.end()));
	}
}
TEST_CASE("Dynamic table functionality")
{
	HPack::DynamicTable test_table(158);
	REQUIRE(test_table.Size() == 0);

	test_table.Insert({ .name = "test1", .value = "1" });
	REQUIRE(test_table.GetEntry(1).name == "test1");
	REQUIRE(test_table.Size() == 38);

	test_table.Insert({ .name = "test2", .value = "2" });
	REQUIRE(test_table.GetEntry(1).name == "test2");
	REQUIRE(test_table.Size() == 76);

	for (int i = 3; i < 13; i++)
	{
		std::string name = "test" + std::to_string(i);
		test_table.Insert({ .name = name, .value = std::to_string(i) });
	}
	REQUIRE(test_table.Size() == 158);
	REQUIRE(test_table.GetEntry(1).name == "test12");
	REQUIRE(test_table.GetOldestEntry().name == "test9");
};
TEST_CASE("Decoding HPack compressed headers")
{
	// Indexed Header Field
	{
		HPack::Codec codec;
		std::vector<uint8_t> encoded_header = { 0x82 };
		std::vector<HPack::TableEntry> header_list;
		REQUIRE(codec.Decode(encoded_header, header_list));

		REQUIRE(codec.GetNumberOfTableEntries() == 0);

		REQUIRE(codec.GetHeaderListSize(header_list) == 1);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == ":method: GET");
	}
	// Literal Header Field With Incrementing Index
	{
		HPack::Codec codec;
		std::vector<uint8_t> encoded_header = { 0x40, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79, 0x0d,
												0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72 };

		std::vector<HPack::TableEntry> header_list;
		REQUIRE(codec.Decode(encoded_header, header_list));
		REQUIRE(codec.GetEntry(62).ToString() == "custom-key: custom-header");

		REQUIRE(codec.GetHeaderListSize(header_list) == 1);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == "custom-key: custom-header");
	}

	// Literal Header Field Without Incrementing Index
	{
		HPack::Codec codec;
		std::vector<uint8_t> encoded_header = { 0x04, 0x0c, 0x2f, 0x73, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2f, 0x70, 0x61, 0x74, 0x68 };
		std::vector<HPack::TableEntry> header_list;
		REQUIRE(codec.Decode(encoded_header, header_list));

		REQUIRE(codec.GetNumberOfTableEntries() == 0);

		REQUIRE(codec.GetHeaderListSize(header_list) == 1);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == ":path: /sample/path");
	}

	// Literal Header Field Never Indexed
	{
		HPack::Codec codec;
		std::vector<uint8_t> encoded_header = { 0x10, 0x08, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72,
												0x64, 0x06, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74 };
		std::vector<HPack::TableEntry> header_list;
		REQUIRE(codec.Decode(encoded_header, header_list));

		REQUIRE(codec.GetNumberOfTableEntries() == 0);

		REQUIRE(codec.GetHeaderListSize(header_list) == 0);
	}
}
TEST_CASE("Processing HTTP2 compressed headers")
{
	// Unencoded headers
	{
		HPack::Codec codec;
		std::vector<uint8_t> first_request = { 0x82, 0x86, 0x84, 0x41, 0x8c, 0xf1, 0xe3, 0xc2, 0xe5,
											   0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff };
		std::vector<HPack::TableEntry> header_list;
		REQUIRE(codec.Decode(first_request, header_list));
		REQUIRE(codec.GetNumberOfTableEntries() == 1);
		REQUIRE(codec.GetTableSize() == 57);
		REQUIRE(codec.GetEntry(62).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetHeaderListSize(header_list) == 4);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == ":method: GET");
		REQUIRE(codec.GetHeaderListEntry(header_list, 1).ToString() == ":scheme: http");
		REQUIRE(codec.GetHeaderListEntry(header_list, 2).ToString() == ":path: /");
		REQUIRE(codec.GetHeaderListEntry(header_list, 3).ToString() == ":authority: www.example.com");

		header_list.clear();
		std::vector<uint8_t> second_request = { 0x82, 0x86, 0x84, 0xbe, 0x58, 0x08, 0x6e, 0x6f, 0x2d, 0x63, 0x61, 0x63, 0x68, 0x65 };
		REQUIRE(codec.Decode(second_request, header_list));
		REQUIRE(codec.GetNumberOfTableEntries() == 2);
		REQUIRE(codec.GetTableSize() == 110);
		REQUIRE(codec.GetEntry(62).ToString() == "cache-control: no-cache");
		REQUIRE(codec.GetEntry(63).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetHeaderListSize(header_list) == 5);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == ":method: GET");
		REQUIRE(codec.GetHeaderListEntry(header_list, 1).ToString() == ":scheme: http");
		REQUIRE(codec.GetHeaderListEntry(header_list, 2).ToString() == ":path: /");
		REQUIRE(codec.GetHeaderListEntry(header_list, 3).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetHeaderListEntry(header_list, 4).ToString() == "cache-control: no-cache");

		header_list.clear();
		std::vector<uint8_t> third_request = { 0x82, 0x87, 0x85, 0xbf, 0x40, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
											   0x79, 0x0c, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65 };
		REQUIRE(codec.Decode(third_request, header_list));
		REQUIRE(codec.GetNumberOfTableEntries() == 3);
		REQUIRE(codec.GetEntry(62).ToString() == "custom-key: custom-value");
		REQUIRE(codec.GetEntry(63).ToString() == "cache-control: no-cache");
		REQUIRE(codec.GetEntry(64).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetTableSize() == 164);
		REQUIRE(codec.GetHeaderListSize(header_list) == 5);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == ":method: GET");
		REQUIRE(codec.GetHeaderListEntry(header_list, 1).ToString() == ":scheme: https");
		REQUIRE(codec.GetHeaderListEntry(header_list, 2).ToString() == ":path: /index.html");
		REQUIRE(codec.GetHeaderListEntry(header_list, 3).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetHeaderListEntry(header_list, 4).ToString() == "custom-key: custom-value");
	}
	// Huffman Encoded headers
	{
		HPack::Codec codec;
		std::vector<uint8_t> first_request = { 0x82, 0x86, 0x84, 0x41, 0x8c, 0xf1, 0xe3, 0xc2, 0xe5,
											   0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff };
		std::vector<HPack::TableEntry> header_list;
		REQUIRE(codec.Decode(first_request, header_list));
		REQUIRE(codec.GetNumberOfTableEntries() == 1);
		REQUIRE(codec.GetTableSize() == 57);
		REQUIRE(codec.GetEntry(62).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetHeaderListSize(header_list) == 4);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == ":method: GET");
		REQUIRE(codec.GetHeaderListEntry(header_list, 1).ToString() == ":scheme: http");
		REQUIRE(codec.GetHeaderListEntry(header_list, 2).ToString() == ":path: /");
		REQUIRE(codec.GetHeaderListEntry(header_list, 3).ToString() == ":authority: www.example.com");

		header_list.clear();
		std::vector<uint8_t> second_request = { 0x82, 0x86, 0x84, 0xbe, 0x58, 0x86, 0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf };
		REQUIRE(codec.Decode(second_request, header_list));
		REQUIRE(codec.GetNumberOfTableEntries() == 2);
		REQUIRE(codec.GetTableSize() == 110);
		REQUIRE(codec.GetEntry(62).ToString() == "cache-control: no-cache");
		REQUIRE(codec.GetEntry(63).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetHeaderListSize(header_list) == 5);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == ":method: GET");
		REQUIRE(codec.GetHeaderListEntry(header_list, 1).ToString() == ":scheme: http");
		REQUIRE(codec.GetHeaderListEntry(header_list, 2).ToString() == ":path: /");
		REQUIRE(codec.GetHeaderListEntry(header_list, 3).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetHeaderListEntry(header_list, 4).ToString() == "cache-control: no-cache");

		header_list.clear();
		std::vector<uint8_t> third_request = { 0x82, 0x87, 0x85, 0xbf, 0x40, 0x88, 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9,
											   0x7d, 0x7f, 0x89, 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf };
		REQUIRE(codec.Decode(third_request, header_list));
		REQUIRE(codec.GetNumberOfTableEntries() == 3);
		REQUIRE(codec.GetEntry(62).ToString() == "custom-key: custom-value");
		REQUIRE(codec.GetEntry(63).ToString() == "cache-control: no-cache");
		REQUIRE(codec.GetEntry(64).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetTableSize() == 164);
		REQUIRE(codec.GetHeaderListSize(header_list) == 5);
		REQUIRE(codec.GetHeaderListEntry(header_list, 0).ToString() == ":method: GET");
		REQUIRE(codec.GetHeaderListEntry(header_list, 1).ToString() == ":scheme: https");
		REQUIRE(codec.GetHeaderListEntry(header_list, 2).ToString() == ":path: /index.html");
		REQUIRE(codec.GetHeaderListEntry(header_list, 3).ToString() == ":authority: www.example.com");
		REQUIRE(codec.GetHeaderListEntry(header_list, 4).ToString() == "custom-key: custom-value");
	}
}
