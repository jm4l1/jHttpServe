#ifndef __HTTP2_H__
#define __HTTP2_H__

#include <bitset>
#include <cstring>
#include <future>
#include <iostream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <variant>
#include <vector>

// Frame Types
// enum class Http2FrameType : uint8_t
// {
// 	DATA = 0x00,
// 	HEADER = 0x01,
// 	PRIORITY = 0x02,
// 	RST_STREAM = 0x03,
// 	SETTINGS = 0x04,
// 	PUSH_PROMISE = 0x05,
// 	PING = 0x06,
// 	GOAWAY = 0x07,
// 	WINDOW_UPDATE = 0x08,
// 	CONTINUATION = 0x09,
// };
#define HTTP2_DATA_FRAME 0x0
#define HTTP2_HEADERS_FRAME 0x1
#define HTTP2_PRIORITY_FRAME 0x2
#define HTTP2_RST_STREAM_FRAME 0x3
#define HTTP2_SETTINGS_FRAME 0x4
#define HTTP2_PUSH_PROMISE_FRAME 0x5
#define HTTP2_PING_FRAME 0x6
#define HTTP2_GOAWAY_FRAME 0x7
#define HTTP2_WINDOW_UPDATE_FRAME 0x8
#define HTTP2_CONTINUATION_FRAME 0x9

// Flags
#define HTTP2_FLAG_UNUSED 0x00
#define HTTP2_FLAG_PADDED 0x08
#define HTTP2_FLAG_HEADERS 0x04

#define HTTP2_DATA_FLAG_END_STREAM 0x01

#define HTTP2_HEADERS_FLAG_END_STREAM 0x01
#define HTTP2_HEADERS_FLAG_PRIORITY 0x20

#define HTTP2_PING_FLAG_ACK 0x01

#define HTTP2_SETTINGS_FLAG_ACK 0x01


// Settings Parameters
#define SETTINGS_HEADER_TABLE_SIZE 0x0001
#define SETTINGS_ENABLE_PUSH 0x0002
#define SETTINGS_MAX_CONCURRENT_STREAMS 0x0003
#define SETTINGS_INITIAL_WINDOW_SIZE 0x0004
#define SETTINGS_MAX_FRAME_SIZE 0x0005
#define SETTINGS_MAX_HEADER_LIST_SIZE 0x0006

// Data sizes
constexpr size_t HTTP2_MAGIC_PREFACE_LENGTH = 24;
constexpr size_t HTTP2_FRAME_HEADER_LENGTH = 9;

// Error Codes
enum class Http2Error : uint8_t
{
	NO_ERROR = 0x00,
	PROTOCOL_ERROR = 0x01,
	INTERNAL_ERROR = 0x02,
	FLOW_CONTROL_ERROR = 0x03,
	SETTINGS_TIMEOUT = 0x04,
	STREAM_CLOSED = 0x05,
	FRAME_SIZE_ERROR = 0x06,
	REFUSED_STREAM = 0x07,
	CANCEL = 0x08,
	COMPRESSION_ERROR = 0x09,
	CONNECT_ERROR = 0x0a,
	ENHANCE_YOUR_CALM = 0x0b,
	INADEQUATE_SECURITY = 0x0c,
	HTTP_1_1_REQUIRED = 0x0d
};

struct Http2Frame;

struct ErrorType
{
	Http2Error _error;
	std::string _reason;
};

using Http2WindowUpdateFrameParseResult = std::variant<uint32_t, ErrorType>;
using Http2ParseResult = std::variant<Http2Frame, ErrorType>;

#define CONNECTION_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"

#pragma pack(push, 1)
struct Http2Frame
{
#if 0
        //                              Http2  Frame Header                         
        3 bytes            1 byte  1 byte   1  31 bits              0  -  2^24 -1 
        +=========================================================================+
        |    length      |  Type  | Flags  |R|       Stream ID      |   payload    |
        +=========================================================================+
#endif
	static Http2ParseResult GetFromBuffer(const std::vector<unsigned char>& data_buffer);
	Http2Frame() = default;
	bool IsValidSettingsFrame() const;
	bool IsSettingsWithAck() const;
	std::bitset<24> length;
	std::bitset<8> type;
	std::bitset<8> flags;
	std::bitset<32> stream_id;
	std::vector<unsigned char> payload = {};
	std::vector<unsigned char> Serialize() const
	{
		std::vector<unsigned char> buffer;
		buffer.reserve(9 + payload.size());
		auto pos = 0;
		while (pos < length.size())
		{
			std::bitset<8> byte;
			byte[0] = length[pos];
			byte[1] = length[pos + 1];
			byte[2] = length[pos + 2];
			byte[3] = length[pos + 3];
			byte[4] = length[pos + 4];
			byte[5] = length[pos + 5];
			byte[6] = length[pos + 6];
			byte[7] = length[pos + 7];
			buffer.insert(buffer.begin(), byte.to_ulong());
			pos += 8;
		}
		pos = 0;
		while (pos < type.size())
		{
			std::bitset<8> byte;
			byte[0] = type[pos];
			byte[1] = type[pos + 1];
			byte[2] = type[pos + 2];
			byte[3] = type[pos + 3];
			byte[4] = type[pos + 4];
			byte[5] = type[pos + 5];
			byte[6] = type[pos + 6];
			byte[7] = type[pos + 7];
			buffer.emplace_back(byte.to_ulong());
			pos += 8;
		}
		pos = 0;
		while (pos < flags.size())
		{
			std::bitset<8> byte;
			byte[0] = flags[pos];
			byte[1] = flags[pos + 1];
			byte[2] = flags[pos + 2];
			byte[3] = flags[pos + 3];
			byte[4] = flags[pos + 4];
			byte[5] = flags[pos + 5];
			byte[6] = flags[pos + 6];
			byte[7] = flags[pos + 7];
			buffer.emplace_back(byte.to_ulong());
			pos += 8;
		}
		pos = 0;
		while (pos < stream_id.size())
		{
			std::bitset<8> byte;
			byte[0] = stream_id[pos];
			byte[1] = stream_id[pos + 1];
			byte[2] = stream_id[pos + 2];
			byte[3] = stream_id[pos + 3];
			byte[4] = stream_id[pos + 4];
			byte[5] = stream_id[pos + 5];
			byte[6] = stream_id[pos + 6];
			byte[7] = stream_id[pos + 7];
			buffer.insert(buffer.begin() + 5, byte.to_ulong());
			pos += 8;
		}
		buffer.insert(buffer.end(), payload.begin(), payload.end());
		// auto payload_length = payload.size();
		// ssize_t first = 0;
		// while (first < payload_length)
		// {
		// 	std::ostringstream os;
		// 	ssize_t last = first + 8;
		// 	std::copy(payload.begin() + first, payload.begin() + last, std::ostream_iterator<bool>(os, ""));
		// 	std::bitset<8> byte(os.str());
		// 	buffer.insert(buffer.end(), byte.to_ulong());
		// 	first = last;
		// }payload  size
		return buffer;
	}
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Http2SettingsParam
{
#if 0
        //                              Http2  Settings Param                         
        2 bytes               4 bytes                                               
        +=========================================================================+
        |    identifier      |   Value                                            |
        +=========================================================================+
#endif
	Http2SettingsParam(std::bitset<16> id, std::bitset<32> val)
	  : identifier(id)
	  , value(val)
	{
	}
	std::bitset<16> identifier;
	std::bitset<32> value;
	std::vector<unsigned char> Serialize() const
	{
		std::vector<unsigned char> buffer = {};
		uint16_t id_num = identifier.to_ulong();
		uint32_t val_num = value.to_ulong();

		buffer.reserve(sizeof(id_num) + sizeof(val_num));
		buffer.insert(buffer.end(), (id_num >> 8) & 0xff);
		buffer.insert(buffer.end(), id_num & 0xff);
		buffer.insert(buffer.end(), (val_num >> 24) & 0xff);
		buffer.insert(buffer.end(), (val_num >> 16) & 0xff);
		buffer.insert(buffer.end(), (val_num >> 8) & 0xff);
		buffer.insert(buffer.end(), val_num & 0xff);
		return buffer;
	}
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Http2SettingsFrame
{
	Http2SettingsFrame() = default;
	Http2SettingsFrame(const std::vector<unsigned char>& data_buffer);
	void AddParam(const Http2SettingsParam param)
	{
		params.push_back(param);
	}
	std::vector<unsigned char> Serialize() const
	{
		std::vector<unsigned char> buffer;
		for (auto param : params)
		{
			auto param_buffer = param.Serialize();
			buffer.insert(buffer.begin(), param_buffer.begin(), param_buffer.end());
		}
		return buffer;
	}
	ssize_t Size() const
	{
		return params.size() * 6;
	}
	std::vector<Http2SettingsParam> params;
};
#pragma pack(pop)

#endif