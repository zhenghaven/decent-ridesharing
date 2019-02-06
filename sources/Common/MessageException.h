#pragma once

#include <exception>

class MessageException : public std::exception
{
public:
	MessageException()
	{}
	virtual ~MessageException()
	{}

	virtual const char* what() const throw()
	{
		return "General Message Exception.";
	}
private:

};

class MessageParseException : public MessageException
{
public:
	MessageParseException()
	{}
	virtual ~MessageParseException()
	{}

	virtual const char* what() const throw()
	{
		return "Message Parse Error. Invalid message format!";
	}
private:

};
