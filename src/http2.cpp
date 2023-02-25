#include "Http2.h"

#include <cstring>
#include <iostream>
#include <string>

Http2Frame Http2Frame::GetFromBuffer(const std::vector<unsigned char>& data_buffer)
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
	offset++;
	memcpy(&frame.flags, data_buffer.data() + offset, 1);
	offset++;
	memcpy(&data, data_buffer.data() + offset, 4);
	frame.stream_id = htonl(data);
	offset += 4;
	frame.payload.insert(frame.payload.begin(), data_buffer.begin() + offset, data_buffer.begin() + 9 + frame.length.to_ulong());
	return frame;
}

Http2Frame::Http2Frame(const std::vector<unsigned char>& data_buffer)
{
	if (data_buffer.size() < 9)
	{
		throw ::std::length_error("data buffer not large enough to contain header");
	}
	size_t offset = 0;
	length = std::bitset<24>(std::string(data_buffer.begin() + offset, data_buffer.begin() + length.size()));
	if (length != data_buffer.size() - 9)
	{
		throw ::std::length_error("Frame size does not match length");
	}
	offset += length.size();
	type = data_buffer[offset];
	offset += type.size();
	flags = data_buffer[offset];
	offset += flags.size();
	stream_id = std::bitset<32>(std::string(data_buffer.begin() + offset, data_buffer.begin() + stream_id.size()));
	offset += stream_id.size();
	payload.insert(payload.begin(), data_buffer.begin() + offset, data_buffer.end());
}

bool Http2Frame::IsValidSettingsFrame() const
{
	if (type != HTTP2_SETTINGS_FRAME)
	{
		return false;
	}
	if (length.to_ulong() % 6 != 0)
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