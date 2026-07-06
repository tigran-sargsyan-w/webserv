#include "UploadHandler.hpp"
#include "ErrorResponseHandler.hpp"
#include "MultipartParser.hpp"
#include "PathUtils.hpp"
#include "StoragePathResolver.hpp"
#include "UploadStorage.hpp"

#include <map>
#include <string>

namespace
{
	bool	hasExplicitBodyFraming(const Request &request)
	{
		const std::map<std::string, std::string> &headers = request.getHeaders();

		return (headers.find("content-length") != headers.end()
			|| headers.find("transfer-encoding") != headers.end());
	}

	Response	validateUploadRequest(const Request &request,
		const RouteConfig &route, const ServerConfig &server)
	{
		Response	response;

		if (!route.uploadEnable)
			return (ErrorResponseHandler::build(403,
				"Forbidden: Upload is disabled for this route", server));
		if (route.uploadStore.empty())
			return (ErrorResponseHandler::build(500,
				"Internal Server Error: Upload store not configured", server));
		if (request.getBody().empty() && !hasExplicitBodyFraming(request))
			return (ErrorResponseHandler::build(400,
				"Bad request: Empty body", server));
		response.setStatusCode(0);
		return (response);
	}

	Response	handleMultipartUpload(const Request &request,
		const RouteConfig &route, const ServerConfig &server)
	{
		MultipartParser::UploadedFile	uploaded;

		uploaded = MultipartParser::parseFileUpload(request);
		if (!uploaded.valid)
			return (ErrorResponseHandler::build(400,
				"Bad Request: Invalid multipart file", server));
		return (UploadStorage::save(route.uploadStore, uploaded.fileName,
			uploaded.content, server));
	}

	Response	handleRawUpload(const Request &request,
		const RouteConfig &route, const ServerConfig &server)
	{
		std::string	fullPath;
		std::string	fileName;

		fullPath = StoragePathResolver::resolve(request, route);
		if (fullPath.empty())
			return (ErrorResponseHandler::build(400,
				"Bad Request: Invalid file name", server));
		fileName = PathUtils::getFileName(fullPath);
		return (UploadStorage::save(route.uploadStore, fileName,
			request.getBody(), server));
	}
}

Response	UploadHandler::handle(const Request &request,
	const RouteConfig &route, const ServerConfig &server)
{
	Response	validation;

	validation = validateUploadRequest(request, route, server);
	if (validation.getStatusCode() != 0)
		return (validation);
	if (MultipartParser::isMultipartRequest(request))
		return (handleMultipartUpload(request, route, server));
	return (handleRawUpload(request, route, server));
}
