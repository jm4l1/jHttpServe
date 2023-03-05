#ifndef __STREAM_H__
#define __STREAM_H__

#include "Http2.h"
#include "HttpMessage.h"

enum class StreamState
{
	Idle,
	ReservedLocal,
	ReservedRemote,
	Open,
	HalfClosedRemote,
	HalfClosedLocal,
	Closed
};

enum class StreamAction
{
	Send,
	Receive
};

enum class StreamInitiator
{
	Client,
	Server
};

class Stream
{
public:
	Stream() = default;
	Stream(Stream&& other);
	Stream& operator=(Stream&& other);
	Stream(uint32_t stream_id, StreamInitiator initiator);
	StreamState GetState() const;
	uint32_t GetStreamId() const;
	Http2Error TransitionToNextState(StreamAction action, uint8_t frame_type, uint8_t flags = 0x00);
	void SetOriginalRequest(HttpRequest original_request);

private:
	uint32_t _stream_id;
	StreamState _state = StreamState::Idle;
	StreamInitiator _initiator;
	HttpRequest _original_request;
};
#endif