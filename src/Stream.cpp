#include "Stream.h"


Stream::Stream(uint32_t stream_id, StreamInitiator initiator)
  : _stream_id(stream_id)
  , _initiator(initiator)
{
	if (_stream_id == 0x0000)
	{
		throw std::logic_error("Stream ID 0x0000 can not be used to initiate a new stream");
	}
}

Stream::Stream(Stream&& other)
{
	this->_stream_id = other._stream_id;
	this->_state = other._state;
	this->_initiator = other._initiator;
	this->_original_request = std::move(other._original_request);
}

Stream& Stream::operator=(Stream&& other)
{
	this->_stream_id = other._stream_id;
	this->_state = other._state;
	this->_initiator = other._initiator;
	this->_original_request = std::move(other._original_request);
	return *this;
}

StreamState Stream::GetState() const
{
	return _state;
}

uint32_t Stream::GetStreamId() const
{
	return _stream_id;
};

Http2Error Stream::TransitionToNextState(StreamAction action, uint8_t frame_type, uint8_t flags)
{
	switch (_state)
	{
	case StreamState::Idle:
	{
		if (frame_type != HTTP2_HEADERS_FRAME && frame_type != HTTP2_PUSH_PROMISE_FRAME)
		{
			return Http2Error::PROTOCOL_ERROR;
		}
		if (frame_type == HTTP2_HEADERS_FRAME)
		{
			if (action == StreamAction::Receive && _initiator == StreamInitiator::Server)
			{
				return Http2Error::PROTOCOL_ERROR;
			}
			_state = StreamState::Open;
			return Http2Error::NO_ERROR;
		}
		if (action == StreamAction::Send)
		{
			_state = StreamState::ReservedLocal;
			return Http2Error::NO_ERROR;
		}
		_state = StreamState::ReservedRemote;
		return Http2Error::NO_ERROR;
	}
	case StreamState::ReservedLocal:
	{
		if (frame_type == HTTP2_RST_STREAM_FRAME)
		{
			_state = StreamState::Closed;
			return Http2Error::NO_ERROR;
		}
		if (action == StreamAction::Send && frame_type == HTTP2_HEADERS_FRAME)
		{
			_state = StreamState::HalfClosedRemote;
			return Http2Error::NO_ERROR;
		}
		if (frame_type == HTTP2_PRIORITY_FRAME && (action == StreamAction::Receive || frame_type == HTTP2_WINDOW_UPDATE_FRAME))
		{
			return Http2Error::NO_ERROR;
		}
		if (action == StreamAction::Send)
		{
			throw std::logic_error("Invalid state for sending frame type");
		}
		return Http2Error::PROTOCOL_ERROR;
	}
	case StreamState::ReservedRemote:
	{
		if (frame_type == HTTP2_RST_STREAM_FRAME)
		{
			_state = StreamState::Closed;
			return Http2Error::NO_ERROR;
		}
		if (action == StreamAction::Receive && frame_type == HTTP2_HEADERS_FRAME)
		{
			_state = StreamState::HalfClosedRemote;
			return Http2Error::NO_ERROR;
		}
		if (frame_type == HTTP2_PRIORITY_FRAME && (action == StreamAction::Send || frame_type == HTTP2_WINDOW_UPDATE_FRAME))
		{
			return Http2Error::NO_ERROR;
		}
		if (action == StreamAction::Send)
		{
			throw std::logic_error("Invalid state for sending frame type");
		}
		return Http2Error::PROTOCOL_ERROR;
	}
	case StreamState::Open:
	{
		if (frame_type == HTTP2_RST_STREAM_FRAME)
		{
			_state = StreamState::Closed;
		}
		if (flags == HTTP2_HEADERS_FLAG_END_STREAM || flags == HTTP2_DATA_FLAG_END_STREAM)
		{
			_state = action == StreamAction::Receive ? StreamState::HalfClosedRemote : StreamState::HalfClosedLocal;
		}
		return Http2Error::NO_ERROR;
	}
	case StreamState::HalfClosedLocal:
	{
		if (frame_type == HTTP2_RST_STREAM_FRAME ||
			(action == StreamAction::Send && (flags == HTTP2_HEADERS_FLAG_END_STREAM || flags == HTTP2_DATA_FLAG_END_STREAM)))
		{
			_state = StreamState::Closed;
			return Http2Error::NO_ERROR;
		}
		if (action == StreamAction::Send && !(frame_type == HTTP2_WINDOW_UPDATE_FRAME || frame_type == HTTP2_PRIORITY_FRAME))
		{
			throw std::logic_error("Invalid state for sending frame type");
		}
		return Http2Error::NO_ERROR;
	}
	case StreamState::HalfClosedRemote:
	{
		if (frame_type == HTTP2_RST_STREAM_FRAME ||
			(action == StreamAction::Receive && (flags == HTTP2_HEADERS_FLAG_END_STREAM || flags == HTTP2_DATA_FLAG_END_STREAM)))
		{
			_state = StreamState::Closed;
			return Http2Error::NO_ERROR;
		}
		if (action == StreamAction::Receive && !(frame_type == HTTP2_WINDOW_UPDATE_FRAME || frame_type == HTTP2_PRIORITY_FRAME))
		{
			throw std::logic_error("Invalid state for receiving frame type");
		}
		return Http2Error::NO_ERROR;
	}
	case StreamState::Closed:
	{
		if (frame_type == HTTP2_PRIORITY_FRAME)
		{
			return Http2Error::NO_ERROR;
		}
		if (action == StreamAction::Send)
		{
			throw std::logic_error("Invalid state for receiving frame type");
		}

		return Http2Error::STREAM_CLOSED;
	}
	}
	return Http2Error::NO_ERROR;
};

void Stream::SetOriginalRequest(HttpRequest original_request)
{
	_original_request = HttpRequest(original_request);
};
