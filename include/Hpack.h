#ifndef __HPACK_H_
#define __HPACK_H_

/*
	Defines the HPACK standard for HTTP header compression defined in
	https://www.rfc-editor.org/rfc/rfc7541
*/

#include "Huffman.h"

#include <algorithm>
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

	static Table static_table = { { 1, { .name = ":authority", .value = "" } },
								  { 2, { .name = ":method", .value = "GET" } },
								  { 3, { .name = ":method", .value = "POST" } },
								  { 4, { .name = ":path", .value = "/" } },
								  { 5, { .name = ":path", .value = "/index.html" } },
								  { 6, { .name = ":scheme", .value = "http" } },
								  { 7, { .name = ":scheme", .value = "https" } },
								  { 8, { .name = ":status", .value = "200" } },
								  { 9, { .name = ":status", .value = "204" } },
								  { 10, { .name = ":status", .value = "206" } },
								  { 11, { .name = ":status", .value = "304" } },
								  { 12, { .name = ":status", .value = "400" } },
								  { 13, { .name = ":status", .value = "404" } },
								  { 14, { .name = ":status", .value = "500" } },
								  { 15, { .name = "accept-charset", .value = "" } },
								  { 16, { .name = "accept-encoding", .value = "gzip, deflate" } },
								  { 17, { .name = "accept-language", .value = "" } },
								  { 18, { .name = "accept-ranges", .value = "" } },
								  { 19, { .name = "accept", .value = "" } },
								  { 20, { .name = "access-control-allow-origin", .value = "" } },
								  { 21, { .name = "age", .value = "" } },
								  { 22, { .name = "allow", .value = "" } },
								  { 23, { .name = "authorization", .value = "" } },
								  { 24, { .name = "cache-control", .value = "" } },
								  { 25, { .name = "content-disposition", .value = "" } },
								  { 26, { .name = "content-encoding", .value = "" } },
								  { 27, { .name = "content-language", .value = "" } },
								  { 28, { .name = "content-length", .value = "" } },
								  { 29, { .name = "content-location", .value = "" } },
								  { 30, { .name = "content-range", .value = "" } },
								  { 31, { .name = "content-type", .value = "" } },
								  { 32, { .name = "cookie", .value = "" } },
								  { 33, { .name = "date", .value = "" } },
								  { 34, { .name = "etag", .value = "" } },
								  { 35, { .name = "expect", .value = "" } },
								  { 36, { .name = "expires", .value = "" } },
								  { 37, { .name = "from", .value = "" } },
								  { 38, { .name = "host", .value = "" } },
								  { 39, { .name = "if-match", .value = "" } },
								  { 40, { .name = "if-modified-since", .value = "" } },
								  { 41, { .name = "if-none-match", .value = "" } },
								  { 42, { .name = "if-range", .value = "" } },
								  { 43, { .name = "if-unmodified-since", .value = "" } },
								  { 44, { .name = "last-modified", .value = "" } },
								  { 45, { .name = "link", .value = "" } },
								  { 46, { .name = "location", .value = "" } },
								  { 47, { .name = "max-forwards", .value = "" } },
								  { 48, { .name = "proxy-authenticate", .value = "" } },
								  { 49, { .name = "proxy-authorization", .value = "" } },
								  { 50, { .name = "range", .value = "" } },
								  { 51, { .name = "referer", .value = "" } },
								  { 52, { .name = "refresh", .value = "" } },
								  { 53, { .name = "retry-after", .value = "" } },
								  { 54, { .name = "server", .value = "" } },
								  { 55, { .name = "set-cookie", .value = "" } },
								  { 56, { .name = "strict-transport-security", .value = "" } },
								  { 57, { .name = "transfer-encoding", .value = "" } },
								  { 58, { .name = "user-agent", .value = "" } },
								  { 59, { .name = "vary", .value = "" } },
								  { 60, { .name = "via", .value = "" } },
								  { 61, { .name = "www-authenticate", .value = "" } } };

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

	private:
		std::vector<TableEntry> _table;
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