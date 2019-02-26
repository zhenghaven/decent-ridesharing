#pragma once

#include "RuntimeException.h"

namespace RideShare
{
	class MessageException : public RuntimeException
	{
	public:
		using RuntimeException::RuntimeException;

	private:

	};

	class MessageParseException : public MessageException
	{
	public:
		MessageParseException() :
			MessageException("Message Parse Error. Invalid message format!")
		{}

	private:

	};
}
