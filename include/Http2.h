#ifndef __HTTP2_H__
#define __HTTP2_H__

#include <bitset>
#include <future>
#include <iterator>
#include <ostream>
#include <sstream>
#include <vector>

// Frame Types
#define HTTP2_DATA_FRAME 0x0
#define HTTP2_HEADER_FRAME 0x1
#define HTTP2_PRIORITY_FRAME 0x2
#define HTTP2_RST_STREAM_FRAME 0x3
#define HTTP2_SETTINGS_FRAME 0x4
#define HTTP2_PUSH_PROMISE_FRAME 0x5
#define HTTP2_PING_FRAME 0x6
#define HTTP2_GOAWAY_FRAME 0x7
#define HTTP2_WINDOW_UPDATE_FRAME 0x8
#define HTTP2_CONTINUATION_FRAME 0x8

// Settings Paramerters
#define SETTINGS_HEADER_TABLE_SIZE 0x0001
#define SETTINGS_ENABLE_PUSH 0x0002
#define SETTINGS_MAX_CONCURRENT_STREAMS 0x0003
#define SETTINGS_INITIAL_WINDOW_SIZE 0x0004
#define SETTINGS_MAX_FRAME_SIZE 0x0005
#define SETTINGS_MAX_HEADER_LIST_SIZE 0x0006

// Error Codes
#define HTTP2_ERROR_NO_ERROR 0x0
#define HTTP2_ERROR_PROTOCOL_ERROR 0x1
#define HTTP2_ERROR_INTERNAL_ERROR 0x2
#define HTTP2_ERROR_FLOW_CONTROL_ERROR 0x3
#define HTTP2_ERROR_SETTINGS_TIMEOUT 0x4
#define HTTP2_ERROR_STREAM_CLOSED 0x5
#define HTTP2_ERROR_FRAME_SIZE_ERROR 0x6
#define HTTP2_ERROR_REFUSED_STREAM 0x7
#define HTTP2_ERROR_CANCEL 0x8
#define HTTP2_ERROR_COMPRESSION_ERROR 0x9
#define HTTP2_ERROR_CONNECT_ERROR 0xa
#define HTTP2_ERROR_ENHANCE_YOUR_CALM 0xb
#define HTTP2_ERROR_INADEQUATE_SECURITY 0xc
#define HTTP2_ERROR_HTTP_1_1_REQUIRED 0xd

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
	Http2Frame() = default;
	std::bitset<24> length;
	std::bitset<8> type;
	std::bitset<8> flags;
	std::bitset<32> stream_id;
	std::vector<bool> payload;
	std::vector<unsigned char> Serialize() const
	{
		std::vector<unsigned char> buffer;

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
		auto payload_length = payload.size();
		ssize_t first = 0;
		while (first < payload_length)
		{
			std::ostringstream os;
			ssize_t last = first + 8;
			std::copy(payload.begin() + first, payload.begin() + last, std::ostream_iterator<bool>(os, ""));
			std::bitset<8> byte(os.str());
			buffer.insert(buffer.end(), byte.to_ulong());
			first = last;
		}
		return buffer;
	}
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Http2SettingsParam
{
#if 0
        //                              Http2  Settigns Param                         
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
	std::vector<bool> Serialize() const
	{
		std::vector<bool> buffer;
		int i;
		for (i = identifier.size() - 1; i >= 0; i--)
		{
			buffer.insert(buffer.end(), identifier.test(i));
		}
		for (i = value.size() - 1; i >= 0; i--)
		{
			buffer.insert(buffer.end(), value.test(i));
		}
		return buffer;
	}
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Http2SettingsFrame
{
	void AddParam(const Http2SettingsParam param)
	{
		params.push_back(param);
	}
	std::vector<bool> Serialize() const
	{
		std::vector<bool> buffer;
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

private:
	std::vector<Http2SettingsParam> params;
};
#pragma pack(pop)

#endif