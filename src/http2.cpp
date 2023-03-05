#include "Http2.h"

#include <bitset>
#include <cstring>
#include <iostream>
#include <string>

static auto STREAM_ID_0_REQUIRED(std::bitset<8> type) -> bool
{
	return (type == HTTP2_SETTINGS_FRAME) || (type == HTTP2_PING_FRAME) || (type == HTTP2_GOAWAY_FRAME) ||
		   (type == HTTP2_WINDOW_UPDATE_FRAME);
};


Http2ParseResult Http2Frame::GetFromBuffer(const std::vector<unsigned char>& data_buffer)
{
	Http2Frame frame;
	if (data_buffer.size() < 9)
	{
		throw ::std::length_error("data buffer not large enough to contain header");
	}
	size_t offset = 0;
	uint32_t data;
	memcpy(&data, data_buffer.data() + offset, 3);
	frame.length = htonl(data << 8);
	if (data_buffer.size() < frame.length.to_ulong())
	{
		throw ::std::length_error("Frame size does not match length");
	}
	offset += 3;
	memcpy(&frame.type, data_buffer.data() + offset, 1);
	if (
		// clang-format off
		(frame.type == HTTP2_PRIORITY_FRAME && frame.length != 0x05) || 
		(frame.type == HTTP2_RST_STREAM_FRAME && frame.length != 0x04) ||
		(frame.type == HTTP2_PING_FRAME && frame.length != 0x08) ||
		(frame.type == HTTP2_WINDOW_UPDATE_FRAME && frame.length != 0x04)
		)
	// clang-format on
	{
		ErrorType error = { ._error = Http2Error::PROTOCOL_ERROR, ._reason = "Invalid size for frame type" };
		return error;
	}

	offset++;
	memcpy(&frame.flags, data_buffer.data() + offset, 1);
	offset++;
	memcpy(&data, data_buffer.data() + offset, 4);
	frame.stream_id = htonl(data);

	if ((frame.stream_id != 0x0000 && (STREAM_ID_0_REQUIRED(frame.type))) ||
		(frame.stream_id == 0x0000 && (!STREAM_ID_0_REQUIRED(frame.type))))
	{
		ErrorType error = { ._error = Http2Error::PROTOCOL_ERROR, ._reason = "Invalid stream id for frame type" };
		return error;
	}

	offset += 4;
	frame.payload.insert(frame.payload.begin(), data_buffer.begin() + offset, data_buffer.begin() + 9 + frame.length.to_ulong());
	return frame;
}

bool Http2Frame::IsValidSettingsFrame() const
{
	if (type != HTTP2_SETTINGS_FRAME)
	{
		return false;
	}
	if (length.to_ulong() % 6 != 0x0000)
	{
		return false;
	}
	if (length.to_ulong() > 0 && flags == 0x01)
	{
		return false;
	}
	return true;
}

bool Http2Frame::IsSettingsWithAck() const
{
	return (type == HTTP2_SETTINGS_FRAME && flags == 0x01);
}

Http2SettingsFrame::Http2SettingsFrame(const std::vector<unsigned char>& data_buffer)
{
	if (data_buffer.size() == 0 || data_buffer.size() % 6 != 0)
	{
		throw ::std::length_error("unable to create Setting frame, data buffer length is invalid");
	}
	auto start = data_buffer.begin();
	auto end = start + 6;

	while (end != data_buffer.end())
	{
		size_t offset = 0;
		uint32_t data;
		memcpy(&data, data_buffer.data() + offset, 2);
		offset += 2;
		auto identifier = data;
		memcpy(&data, data_buffer.data() + offset, 4);
		auto value = data;
		Http2SettingsParam param(identifier, value);
		AddParam(param);
		start += offset;
		end = start + 6;
	}
}