#ifndef __HPACK_H_
#define __HPACK_H_

/*
	Defines the HPACK standard for HTTP header compression defined in
	https://www.rfc-editor.org/rfc/rfc7541
*/

#include "Huffman.h"
#include "Utility.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

namespace HPack
{
	constexpr uint8_t STATIC_TABLE_LENGTH = 61;
	constexpr uint8_t HUFFMAN_MIN_CODE_LENGTH = 5;
	constexpr uint8_t LITERAL_HEADER_FIELD_NO_INDEX = 0x00;
	constexpr uint8_t LITERAL_HEADER_FIELD_INCREMENTAL_INDEX = 0x40;
	constexpr uint8_t LITERAL_HEADER_FIELD_NEVER_INDEX = 0x10;
	constexpr uint8_t INDEXED_HEADER_FIELDS = 0x80;
	constexpr uint8_t HUFFMAN_ENCODED = 0x80;

	struct TableEntry
	{
		std::string name;
		std::string value;

		size_t Size() const
		{
			return 32 + name.size() + value.size();
		}
		std::string ToString() const
		{
			return name + ": " + value;
		}
	};
	using Table = std::map<uint8_t, TableEntry>;

	static Table static_table = { { 0x01, { .name = ":authority", .value = "" } },
								  { 0x02, { .name = ":method", .value = "GET" } },
								  { 0x03, { .name = ":method", .value = "POST" } },
								  { 0x04, { .name = ":path", .value = "/" } },
								  { 0x05, { .name = ":path", .value = "/index.html" } },
								  { 0x06, { .name = ":scheme", .value = "http" } },
								  { 0x07, { .name = ":scheme", .value = "https" } },
								  { 0x08, { .name = ":status", .value = "200" } },
								  { 0x09, { .name = ":status", .value = "204" } },
								  { 0x0a, { .name = ":status", .value = "206" } },
								  { 0x0b, { .name = ":status", .value = "304" } },
								  { 0x0c, { .name = ":status", .value = "400" } },
								  { 0x0d, { .name = ":status", .value = "404" } },
								  { 0x0e, { .name = ":status", .value = "500" } },
								  { 0x0f, { .name = "accept-charset", .value = "" } },
								  { 0x10, { .name = "accept-encoding", .value = "gzip, deflate" } },
								  { 0x11, { .name = "accept-language", .value = "" } },
								  { 0x12, { .name = "accept-ranges", .value = "" } },
								  { 0x13, { .name = "accept", .value = "" } },
								  { 0x14, { .name = "access-control-allow-origin", .value = "" } },
								  { 0x15, { .name = "age", .value = "" } },
								  { 0x16, { .name = "allow", .value = "" } },
								  { 0x17, { .name = "authorization", .value = "" } },
								  { 0x18, { .name = "cache-control", .value = "" } },
								  { 0x19, { .name = "content-disposition", .value = "" } },
								  { 0x1a, { .name = "content-encoding", .value = "" } },
								  { 0x1b, { .name = "content-language", .value = "" } },
								  { 0x1c, { .name = "content-length", .value = "" } },
								  { 0x1d, { .name = "content-location", .value = "" } },
								  { 0x1e, { .name = "content-range", .value = "" } },
								  { 0x1f, { .name = "content-type", .value = "" } },
								  { 0x20, { .name = "cookie", .value = "" } },
								  { 0x21, { .name = "date", .value = "" } },
								  { 0x22, { .name = "etag", .value = "" } },
								  { 0x23, { .name = "expect", .value = "" } },
								  { 0x24, { .name = "expires", .value = "" } },
								  { 0x25, { .name = "from", .value = "" } },
								  { 0x26, { .name = "host", .value = "" } },
								  { 0x27, { .name = "if-match", .value = "" } },
								  { 0x28, { .name = "if-modified-since", .value = "" } },
								  { 0x29, { .name = "if-none-match", .value = "" } },
								  { 0x2a, { .name = "if-range", .value = "" } },
								  { 0x2b, { .name = "if-unmodified-since", .value = "" } },
								  { 0x2c, { .name = "last-modified", .value = "" } },
								  { 0x2d, { .name = "link", .value = "" } },
								  { 0x2e, { .name = "location", .value = "" } },
								  { 0x2f, { .name = "max-forwards", .value = "" } },
								  { 0x30, { .name = "proxy-authenticate", .value = "" } },
								  { 0x31, { .name = "proxy-authorization", .value = "" } },
								  { 0x32, { .name = "range", .value = "" } },
								  { 0x33, { .name = "referer", .value = "" } },
								  { 0x34, { .name = "refresh", .value = "" } },
								  { 0x35, { .name = "retry-after", .value = "" } },
								  { 0x36, { .name = "server", .value = "" } },
								  { 0x37, { .name = "set-cookie", .value = "" } },
								  { 0x38, { .name = "strict-transport-security", .value = "" } },
								  { 0x39, { .name = "transfer-encoding", .value = "" } },
								  { 0x3a, { .name = "user-agent", .value = "" } },
								  { 0x3b, { .name = "vary", .value = "" } },
								  { 0x3c, { .name = "via", .value = "" } },
								  { 0x3d, { .name = "www-authenticate", .value = "" } } };

	class DynamicTable
	{
	public:
		DynamicTable() = default;

		explicit DynamicTable(size_t size)
		  : _capacity(size){};

		void UpdateSize(size_t new_capacity)
		{
			_capacity = new_capacity;
		}

		void Insert(const TableEntry& entry)
		{
			if (_table.empty())
			{
				_table.emplace_back(entry);
				return;
			}
			if (Size() + entry.Size() <= _capacity)
			{
				_table.resize(_table.size() + 1);
			}
			else
			{
				while (Size() + entry.Size() > _capacity && _table.size() > 0)
				{
					_table.erase(_table.begin() + _table.size() - 1);
				}
			}
			// shift all elements down 1
			std::vector<TableEntry> sub_table = std::vector(_table.begin(), _table.end() - 1);
			_table[0] = entry;
			std::copy(sub_table.begin(), sub_table.end(), _table.begin() + 1);
		}

		TableEntry GetEntry(const size_t index) const
		{
			auto table_index = index - 1;
			if (table_index > _table.size())
			{
				throw std::runtime_error("Entry not found in table");
			}
			return _table[table_index];
		}

		TableEntry GetOldestEntry() const
		{
			if (_table.empty())
			{
				throw std::runtime_error("Table is empty");
			}
			return _table[_table.size() - 1];
		}

		size_t Size() const
		{
			size_t size = 0;
			std::ranges::for_each(_table,
								  [&size](const auto& entry)
								  {
									  size += entry.Size();
								  });
			return size;
		}

		size_t GetNumberOfEntries() const
		{
			return _table.size();
		}


		std::vector<TableEntry> _table;

	private:
		size_t _capacity = 4096;
	};

	class Codec
	{
	public:
		Codec() = default;

		bool Decode(const std::vector<uint8_t>& encoded_buffer, std::vector<TableEntry>& header_list)
		{
			size_t curr_index = 0;
			while (curr_index < encoded_buffer.size())
			{
				auto curr_byte = *(encoded_buffer.begin() + curr_index);
				if ((curr_byte & INDEXED_HEADER_FIELDS) == INDEXED_HEADER_FIELDS)
				{
					if (!DecodeIndexedHeaderField(curr_byte, header_list))
					{
						return false;
					}
					curr_index++;
					continue;
				}
				if ((curr_byte & LITERAL_HEADER_FIELD_INCREMENTAL_INDEX) == LITERAL_HEADER_FIELD_INCREMENTAL_INDEX)
				{
					if (!DecodeLiteralHeaderFieldWithIncrementalIndex(encoded_buffer, curr_index, header_list))
					{
						return false;
					}
					continue;
				}
				if ((curr_byte & LITERAL_HEADER_FIELD_NEVER_INDEX) == LITERAL_HEADER_FIELD_NEVER_INDEX)
				{
					if (!DecodeLiteralHeaderFieldNeverIndex(encoded_buffer, curr_index, header_list))
					{
						return false;
					}
					continue;
				}
				if ((curr_byte & LITERAL_HEADER_FIELD_NO_INDEX) == LITERAL_HEADER_FIELD_NO_INDEX)
				{
					if (!DecodeLiteralHeaderFieldWithoutIndex(encoded_buffer, curr_index, header_list))
					{
						return false;
					}
					continue;
				}
				return false;
			}
			return true;
		}

		std::vector<uint8_t> LiteralEncode(const std::string& value)
		{
			std::vector<uint8_t> buffer;
			buffer.reserve(value.size());
			buffer.insert(buffer.end(), value.begin(), value.end());
			return buffer;
		}
		std::vector<uint8_t> Encode(const std::string& name, const std::string& value)
		{
			std::string header_name = name;
			Utility::ToLowerCase(header_name);
			std::vector<uint8_t> header_buffer;

			auto matching_static_headers = static_table | std::views::filter(
															  [&header_name](auto& item)
															  {
																  return item.second.name == header_name;
															  });

			// header is in the static list
			if (!matching_static_headers.empty())
			{
				// header in the table only once
				if (matching_static_headers.begin() == matching_static_headers.end())
				{
					auto entry = matching_static_headers.front();
					// header value is in the table
					if (entry.second.value == value)
					{
						uint8_t index = INDEXED_HEADER_FIELDS | entry.first;
						header_buffer.resize(sizeof(index));
						memcpy(header_buffer.data(), &index, sizeof(index));
						return header_buffer;
					}
					// store value in the dynamic table
					uint8_t index;
					auto matched_dynamic_header = _table._table | std::views::filter(
																	  [&header_name](auto& item)
																	  {
																		  return item.name == header_name;
																	  });
					if (!matched_dynamic_header)
					{
						// value not yet in dynamic table
						_table.Insert({ .name = header_name, .value = value });
						index = LITERAL_HEADER_FIELD_INCREMENTAL_INDEX | entry.first;
					}
					else
					{
						if (matched_dynamic_header.begin()->value != value)
						{
							// update indexed value
							matched_dynamic_header.begin()->value = value;
						}
						// use existing index
						index = LITERAL_HEADER_FIELD_INCREMENTAL_INDEX | (std::ranges::find_if(_table._table,
																							   [&value](const auto& item)
																							   {
																								   return item.value == value;
																							   }) -
																		  _table._table.begin());
					}
					bool shouldUseHuffmanEncode = false;  // value.find_first_not_of("0123456789") != std::string::npos;
					auto encoded_value_string = shouldUseHuffmanEncode ? Huffman::Encode(value) : LiteralEncode(value);
					uint8_t value_length = encoded_value_string.size();
					uint8_t huff_value_length =
						shouldUseHuffmanEncode ? HUFFMAN_ENCODED | encoded_value_string.size() : encoded_value_string.size();

					size_t offset = 0;
					header_buffer.resize(sizeof(index) + sizeof(value_length) + value_length);

					memcpy(header_buffer.data() + offset, &index, sizeof(index));
					offset += sizeof(index);
					memcpy(header_buffer.data() + offset, &huff_value_length, sizeof(value_length));
					offset += sizeof(value_length);
					memcpy(header_buffer.data() + offset, encoded_value_string.data(), value_length);
					return header_buffer;
				}

				// multiple occurrences of the header
				auto entry = matching_static_headers.front();
				auto matched_header = matching_static_headers | std::views::filter(
																	[&value](auto& item)
																	{
																		return item.second.value == value;
																	});
				// value is in the static list for given header
				if (matched_header)
				{
					// header value is in the table
					if (entry.second.value == value)
					{
						uint8_t index = INDEXED_HEADER_FIELDS | entry.first;
						header_buffer.resize(sizeof(index));
						memcpy(header_buffer.data(), &index, sizeof(index));
						return header_buffer;
					}
				}
				// store value in the dynamic table
				{
					auto matched_dynamic_header = _table._table | std::views::filter(
																	  [&header_name](auto& item)
																	  {
																		  return item.name == header_name;
																	  });
					uint8_t index;
					if (!matched_dynamic_header)
					{
						_table.Insert({ .name = header_name, .value = value });
						index = entry.first;
					}
					else
					{
						if (matched_dynamic_header.begin()->value != value)
						{
							matched_dynamic_header.begin()->value = value;
						}
						uint8_t i = 0;
						auto itr = std::ranges::find_if(_table._table,
														[&value, &i](const auto& item)
														{
															i++;
															return item.value == value;
														});
						index = i;
					}

					index |= LITERAL_HEADER_FIELD_INCREMENTAL_INDEX;
					bool shouldUseHuffmanEncode = false;  // value.find_first_not_of("0123456789") != std::string::npos;
					auto encoded_value_string = shouldUseHuffmanEncode ? Huffman::Encode(value) : LiteralEncode(value);
					uint8_t value_length = encoded_value_string.size();
					uint8_t huff_value_length =
						shouldUseHuffmanEncode ? HUFFMAN_ENCODED | encoded_value_string.size() : encoded_value_string.size();

					size_t offset = 0;
					header_buffer.resize(sizeof(index) + sizeof(value_length) + value_length);

					memcpy(header_buffer.data() + offset, &index, sizeof(index));
					offset += sizeof(index);
					memcpy(header_buffer.data() + offset, &huff_value_length, sizeof(value_length));
					offset += sizeof(value_length);
					memcpy(header_buffer.data() + offset, encoded_value_string.data(), value_length);

					return header_buffer;
				}
			}
			// store value in the dynamic table
			{
				auto matched_dynamic_header = _table._table | std::views::filter(
																  [&header_name](auto& item)
																  {
																	  return item.name == header_name;
																  });
				uint8_t index;

				if (!matched_dynamic_header)
				{
					_table.Insert({ .name = header_name, .value = value });
					index = _table.GetNumberOfEntries() + STATIC_TABLE_LENGTH;
				}
				else
				{
					if (matched_dynamic_header.begin()->value != value)
					{
						matched_dynamic_header.begin()->value = value;
					}
					index = (std::ranges::find_if(_table._table,
												  [&value](const auto& item)
												  {
													  return item.value == value;
												  }) -
							 _table._table.begin()) +
							STATIC_TABLE_LENGTH;
				}
				index |= LITERAL_HEADER_FIELD_INCREMENTAL_INDEX;
				auto encoded_header_string = Huffman::Encode(header_name);
				bool shouldUseHuffmanEncode = false;  // value.find_first_not_of("0123456789") != std::string::npos;
				auto encoded_value_string = shouldUseHuffmanEncode ? Huffman::Encode(value) : LiteralEncode(value);
				uint8_t name_length = encoded_header_string.size();
				uint8_t huff_name_length =
					shouldUseHuffmanEncode ? HUFFMAN_ENCODED | encoded_header_string.size() : encoded_header_string.size();
				uint8_t value_length = encoded_value_string.size();
				uint8_t huff_value_length =
					shouldUseHuffmanEncode ? HUFFMAN_ENCODED | encoded_value_string.size() : encoded_value_string.size();

				size_t offset = 0;
				header_buffer.resize(sizeof(index) + sizeof(name_length) + name_length + sizeof(value_length) + value_length);
				memcpy(header_buffer.data() + offset, &index, sizeof(index));
				offset += sizeof(index);
				memcpy(header_buffer.data() + offset, &huff_name_length, sizeof(name_length));
				offset += sizeof(name_length);
				memcpy(header_buffer.data() + offset, encoded_header_string.data(), name_length);
				offset += name_length;
				memcpy(header_buffer.data() + offset, &huff_value_length, sizeof(value_length));
				offset += sizeof(value_length);
				memcpy(header_buffer.data() + offset, encoded_value_string.data(), (value_length));

				return header_buffer;
			}
		}

		TableEntry GetEntry(const size_t index) const
		{
			return index < STATIC_TABLE_LENGTH ? static_table[index] : _table.GetEntry(index - STATIC_TABLE_LENGTH);
		}

		size_t GetNumberOfTableEntries() const
		{
			return _table.GetNumberOfEntries();
		}

		size_t GetTableSize() const
		{
			return _table.Size();
		}

		TableEntry GetHeaderListEntry(std::vector<TableEntry>& header_list, size_t position)
		{
			return header_list[position];
		}

		size_t GetHeaderListSize(std::vector<TableEntry>& header_list) const
		{
			return header_list.size();
		}


	private:
		void InsertHeaderListEntry(std::vector<TableEntry>& header_list, const TableEntry& entry)
		{
			if (auto search = std::ranges::find_if(header_list,
												   [&entry](const TableEntry& _entry)
												   {
													   return entry.name == _entry.name;
												   });
				search != header_list.end())
			{
				(*search) = entry;
				return;
			}
			header_list.emplace_back(entry);
		}

		bool DecodeIndexedHeaderField(uint8_t byte, std::vector<TableEntry>& header_list)
		{
			uint8_t index = byte ^ INDEXED_HEADER_FIELDS;
			TableEntry entry = index <= STATIC_TABLE_LENGTH ? static_table[index] : _table.GetEntry(index - STATIC_TABLE_LENGTH);
			InsertHeaderListEntry(header_list, entry);
			return true;
		}

		bool DecodeLiteralHeaderFieldWithIncrementalIndex(const std::vector<uint8_t>& buffer,
														  size_t& curr_index,
														  std::vector<TableEntry>& header_list)
		{
			auto byte = *(buffer.begin() + curr_index);
			uint8_t index = byte ^ LITERAL_HEADER_FIELD_INCREMENTAL_INDEX;
			curr_index++;
			TableEntry entry;
			if (index == 0)
			{
				byte = *(buffer.begin() + curr_index);
				size_t name_length;
				std::string name;
				if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
				{
					name_length = byte;
					curr_index++;
					name = std::string(buffer.data() + curr_index, buffer.data() + curr_index + name_length);
				}
				else
				{
					name_length = byte ^ HUFFMAN_ENCODED;
					curr_index++;
					auto decoded_buffer =
						Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + name_length));
					name = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
				}
				curr_index += name_length;
				byte = *(buffer.begin() + curr_index);
				size_t value_length;
				std::string value;
				if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
				{
					value_length = byte;
					curr_index++;
					value = std::string(buffer.data() + curr_index, buffer.data() + curr_index + value_length);
				}
				else
				{
					value_length = byte ^ HUFFMAN_ENCODED;
					curr_index++;
					auto decoded_buffer =
						Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + value_length));
					value = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
				}
				entry.name = name;
				entry.value = value;
				_table.Insert(entry);
				InsertHeaderListEntry(header_list, entry);
				curr_index += value_length;
				return true;
			}

			entry = index <= STATIC_TABLE_LENGTH ? static_table[index] : _table.GetEntry(index - STATIC_TABLE_LENGTH);
			byte = *(buffer.begin() + curr_index);
			size_t length;
			std::string value;
			if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
			{
				length = byte;
				curr_index++;
				value = std::string(buffer.data() + curr_index, buffer.data() + curr_index + length);
			}
			else
			{
				length = byte ^ HUFFMAN_ENCODED;
				curr_index++;
				auto decoded_buffer =
					Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + length));
				value = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
			}
			entry.value = value;
			_table.Insert(entry);
			InsertHeaderListEntry(header_list, entry);
			curr_index += length;
			return true;
		}

		bool DecodeLiteralHeaderFieldWithoutIndex(const std::vector<uint8_t>& buffer,
												  size_t& curr_index,
												  std::vector<TableEntry>& header_list)
		{
			auto byte = *(buffer.begin() + curr_index);
			uint8_t index = byte ^ LITERAL_HEADER_FIELD_NO_INDEX;
			curr_index++;
			TableEntry entry;
			if (index == 0)
			{
				byte = *(buffer.begin() + curr_index);
				size_t name_length;
				std::string name;
				if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
				{
					name_length = byte;
					curr_index++;
					name = std::string(buffer.data() + curr_index, buffer.data() + curr_index + name_length);
				}
				else
				{
					name_length = byte ^ HUFFMAN_ENCODED;
					curr_index++;
					auto decoded_buffer =
						Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + name_length));
					name = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
				}
				curr_index += name_length;
				byte = *(buffer.begin() + curr_index);
				size_t value_length;
				std::string value;
				if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
				{
					value_length = byte;
					curr_index++;
					value = std::string(buffer.data() + curr_index, buffer.data() + curr_index + value_length);
				}
				else
				{
					value_length = byte ^ HUFFMAN_ENCODED;
					curr_index++;
					auto decoded_buffer =
						Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + value_length));
					value = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
				}
				entry.name = name;
				entry.value = value;
				InsertHeaderListEntry(header_list, entry);
				curr_index += value_length;
				return true;
			}

			entry = index <= STATIC_TABLE_LENGTH ? static_table[index] : _table.GetEntry(index - STATIC_TABLE_LENGTH);
			byte = *(buffer.begin() + curr_index);
			size_t length;
			std::string value;
			if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
			{
				length = byte;
				curr_index++;
				value = std::string(buffer.data() + curr_index, buffer.data() + curr_index + length);
			}
			else
			{
				length = byte ^ HUFFMAN_ENCODED;
				curr_index++;
				auto decoded_buffer =
					Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + length));
				value = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
			}
			entry.value = value;
			InsertHeaderListEntry(header_list, entry);
			curr_index += length;

			return true;
		}

		bool
			DecodeLiteralHeaderFieldNeverIndex(const std::vector<uint8_t>& buffer, size_t& curr_index, std::vector<TableEntry>& header_list)
		{
			auto byte = *(buffer.begin() + curr_index);
			uint8_t index = byte ^ LITERAL_HEADER_FIELD_NEVER_INDEX;
			curr_index++;
			TableEntry entry;
			if (index == 0)
			{
				byte = *(buffer.begin() + curr_index);
				size_t name_length;
				std::string name;
				if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
				{
					name_length = byte;
					curr_index++;
					name = std::string(buffer.data() + curr_index, buffer.data() + curr_index + name_length);
				}
				else
				{
					name_length = byte ^ HUFFMAN_ENCODED;
					curr_index++;
					auto decoded_buffer =
						Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + name_length));
					name = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
				}
				curr_index += name_length;
				byte = *(buffer.begin() + curr_index);
				size_t value_length;
				std::string value;
				if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
				{
					value_length = byte;
					curr_index++;
					value = std::string(buffer.data() + curr_index, buffer.data() + curr_index + value_length);
				}
				else
				{
					value_length = byte ^ HUFFMAN_ENCODED;
					curr_index++;
					auto decoded_buffer =
						Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + value_length));
					value = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
				}
				entry.name = name;
				entry.value = value;
				curr_index += value_length;
				return true;
			}

			entry = index <= STATIC_TABLE_LENGTH ? static_table[index] : _table.GetEntry(index - STATIC_TABLE_LENGTH);
			byte = *(buffer.begin() + curr_index);
			size_t length;
			std::string value;
			if (!((byte & HUFFMAN_ENCODED) == HUFFMAN_ENCODED))
			{
				length = byte;
				curr_index++;
				value = std::string(buffer.data() + curr_index, buffer.data() + curr_index + length);
			}
			else
			{
				length = byte ^ HUFFMAN_ENCODED;
				curr_index++;
				auto decoded_buffer =
					Huffman::Decode(std::vector<uint8_t>(buffer.begin() + curr_index, buffer.begin() + curr_index + length));
				value = std::string(decoded_buffer.data(), decoded_buffer.data() + decoded_buffer.size());
			}
			entry.value = value;
			curr_index += length;
			return true;
		}

	private:
		DynamicTable _table;
	};

}  // namespace HPack
#endif