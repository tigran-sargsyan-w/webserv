#include "ClientEventHandler.hpp"
#include "ClientResponseApplier.hpp"
#include "ErrorResponseHandler.hpp"
#include "Logger.hpp"
#include "RequestDispatcher.hpp"
#include "RequestInspector.hpp"
#include "RequestParser.hpp"
#include "Response.hpp"
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>

namespace
{
	enum ReadEventResult
	{
		READ_EVENT_CONTINUE,
		READ_EVENT_DONE,
		READ_EVENT_CLOSE
	};

	int readFromClient(Client &client)
	{
		ssize_t bytesRead;
		char buffer[4096];

		bytesRead = recv(client.fd, buffer, sizeof(buffer), 0);
		if (bytesRead < 0)
		{
			Logger::error() << "Failed to read from client fd " << client.fd << std::endl;
			client.state = CLOSING_CONNECTION;
			return (1);
		}
		if (bytesRead == 0)
		{
			Logger::debug() << "Client closed connection" << std::endl;
			client.state = CLOSING_CONNECTION;
			return (0);
		}
		client.rawRequest.append(buffer, static_cast<size_t>(bytesRead));
		client.touchActivity();
		return (0);
	}

	int discardRequestBody(Client &client)
	{
		ssize_t bytesRead;
		char buffer[4096];
		size_t readSize;

		if (client.bodyBytesToDiscard == 0)
		{
			client.state = WRITING;
			return (0);
		}
		readSize = sizeof(buffer);
		if (client.bodyBytesToDiscard < readSize)
			readSize = client.bodyBytesToDiscard;
		bytesRead = recv(client.fd, buffer, readSize, 0);
		if (bytesRead < 0)
		{
			Logger::error() << "Failed to discard request body from client fd "
							<< client.fd << std::endl;
			client.state = CLOSING_CONNECTION;
			return (1);
		}
		if (bytesRead == 0)
		{
			client.bodyBytesToDiscard = 0;
			client.state = WRITING;
			return (0);
		}
		if (static_cast<size_t>(bytesRead) >= client.bodyBytesToDiscard)
		{
			client.bodyBytesToDiscard = 0;
			client.state = WRITING;
		}
		else
			client.bodyBytesToDiscard -= static_cast<size_t>(bytesRead);
		client.touchActivity();
		return (0);
	}

	int sendToClient(Client &client, const ServerConfig &server,
					 CgiManager &cgiManager, PollManager &pollManager)
	{
		RequestDispatcher::Result dispatchResult;
		ssize_t bytesSent;
		size_t remaining;
		const char *data;

		if (!client.responseReady)
		{
			dispatchResult = RequestDispatcher::dispatch(client, server,
												 cgiManager, pollManager);
			if (dispatchResult == RequestDispatcher::DISPATCH_FAILED)
				return (1);
			if (dispatchResult == RequestDispatcher::ASYNC_STARTED)
				return (0);
		}
		if (client.bytesSent >= client.responseBuffer.size())
		{
			client.state = CLOSING_CONNECTION;
			return (0);
		}
		remaining = client.responseBuffer.size() - client.bytesSent;
		data = client.responseBuffer.c_str() + client.bytesSent;
		bytesSent = send(client.fd, data, remaining, 0);
		if (bytesSent <= 0)
		{
			Logger::error() << "Failed to send response to client fd "
							<< client.fd << std::endl;
			client.state = CLOSING_CONNECTION;
			return (1);
		}
		client.bytesSent += static_cast<size_t>(bytesSent);
		client.touchActivity();
		if (client.bytesSent >= client.responseBuffer.size())
			client.state = CLOSING_CONNECTION;
		return (0);
	}

	std::string getInspectorErrorMessage(InspectRequestStatus status)
	{
		if (status == BAD_REQUEST)
			return ("Bad Request");
		if (status == REQUEST_TOO_LARGE)
			return ("Payload Too Large");
		if (status == URI_TOO_LONG)
			return ("URI Too Long");
		if (status == HEADER_TOO_LARGE)
			return ("Request Header Fields Too Large");
		if (status == NOT_IMPLEMENTED)
			return ("Not Implemented");
		return ("Bad Request");
	}

	void prepareInspectorErrorResponse(Client &client,
								   const ServerConfig &server, InspectRequestStatus status)
	{
		Response response;
		int statusCode;

		statusCode = static_cast<int>(status);
		if (statusCode < 400 || statusCode > 599)
			statusCode = 400;
		response = ErrorResponseHandler::build(statusCode,
											   getInspectorErrorMessage(status), server);
		ClientResponseApplier::apply(client, response);
	}

	void prepareBodyDiscard(Client &client, const RequestInspection &inspection)
	{
		size_t currentBodySize;

		client.bodyBytesToDiscard = 0;
		if (!inspection.hasContentLength)
			return;
		currentBodySize = 0;
		if (client.rawRequest.length() > inspection.bodyStart)
			currentBodySize = client.rawRequest.length() - inspection.bodyStart;
		if (currentBodySize < inspection.contentLength)
		{
			client.bodyBytesToDiscard = inspection.contentLength - currentBodySize;
			client.state = DISCARDING_BODY;
		}
	}

	ClientEventHandler::Result handleDiscardingBody(Client &client,
											short revents, PollManager &pollManager)
	{
		if (revents & POLLIN)
		{
			discardRequestBody(client);
			if (client.state == CLOSING_CONNECTION)
				return (ClientEventHandler::CLIENT_SHOULD_CLOSE);
			if (client.state == WRITING)
				pollManager.setEvents(client.fd, POLLOUT);
		}
		return (ClientEventHandler::EVENT_HANDLED);
	}

	ClientEventHandler::Result handleActiveCgiClient(Client &client,
											 short revents)
	{
		if (revents & POLLIN)
		{
			readFromClient(client);
			if (client.state == CLOSING_CONNECTION)
				return (ClientEventHandler::CLIENT_SHOULD_CLOSE);
			client.rawRequest.clear();
		}
		return (ClientEventHandler::EVENT_HANDLED);
	}

	ReadEventResult inspectClientRequest(Client &client,
									 const ServerConfig &server, PollManager &pollManager)
	{
		RequestParser parser;
		RequestInspector inspector;
		RequestInspection inspection;

		inspection = inspector.inspectRequest(client.getRawRequest(),
											  server.clientMaxBodySize);
		if (inspection.status == COMPLETED)
		{
			parser.parse(client.getRawRequest(), client.request, inspection);
			pollManager.setEvents(client.fd, POLLOUT);
			return (READ_EVENT_CONTINUE);
		}
		if (inspection.status == NEED_MORE_DATA)
			return (READ_EVENT_DONE);
		prepareInspectorErrorResponse(client, server, inspection.status);
		if (inspection.status == REQUEST_TOO_LARGE)
			prepareBodyDiscard(client, inspection);
		if (client.state == DISCARDING_BODY)
			pollManager.setEvents(client.fd, POLLIN);
		else
			pollManager.setEvents(client.fd, POLLOUT);
		return (READ_EVENT_DONE);
	}

	ReadEventResult handleReadEvent(Client &client,
								const ServerConfig &server, PollManager &pollManager)
	{
		client.state = READING;
		readFromClient(client);
		if (client.state == CLOSING_CONNECTION)
			return (READ_EVENT_CLOSE);
		return (inspectClientRequest(client, server, pollManager));
	}

	ClientEventHandler::Result handleWriteEvent(Client &client,
										const ServerConfig &server, CgiManager &cgiManager,
										PollManager &pollManager)
	{
		client.state = WRITING;
		sendToClient(client, server, cgiManager, pollManager);
		if (client.state == CLOSING_CONNECTION)
			return (ClientEventHandler::CLIENT_SHOULD_CLOSE);
		return (ClientEventHandler::EVENT_HANDLED);
	}
}

ClientEventHandler::Result ClientEventHandler::handle(Client &client,
											  short revents, const ServerConfig &server, CgiManager &cgiManager,
											  PollManager &pollManager)
{
	ReadEventResult readResult;

	if (client.state == DISCARDING_BODY)
		return (handleDiscardingBody(client, revents, pollManager));
	if (client.state == CGI_WRITING || client.state == CGI_READING)
		return (handleActiveCgiClient(client, revents));
	if (revents & POLLIN)
	{
		readResult = handleReadEvent(client, server, pollManager);
		if (readResult == READ_EVENT_CLOSE)
			return (CLIENT_SHOULD_CLOSE);
		if (readResult == READ_EVENT_DONE)
			return (EVENT_HANDLED);
		if (readResult == READ_EVENT_CONTINUE)
			return (EVENT_HANDLED);
	}
	if (revents & POLLOUT)
		return (handleWriteEvent(client, server, cgiManager, pollManager));
	return (EVENT_HANDLED);
}
