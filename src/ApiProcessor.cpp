#include "ApiProcessor.hpp"

#include <AuthManager.hpp>
#include <Session.hpp>

#include <misc/utils.hpp>

#include <nlohmann/json.hpp>
#include <uWS.h>

using Endpoint = ApiProcessor::Endpoint;

using TemplatedEndpointBuilder = ApiProcessor::TemplatedEndpointBuilder;

template<typename... Args>
using TemplatedEndpoint = ApiProcessor::TemplatedEndpoint<Args...>;

ApiProcessor::ApiProcessor(uWS::Hub& h, AuthManager& am)
: am(am) {
	// one connection can request multiple things before it closes
	h.onHttpRequest([this] (uWS::HttpResponse * res, uWS::HttpRequest req, char * data, sz_t len, sz_t rem) {
		ll::shared_ptr<Request> * rs = static_cast<ll::shared_ptr<Request> *>(res->getHttpSocket()->getUserData());

		if (!rs) {
			// we want to crash if this throws anyways, so no leak should be possible
			rs = new ll::shared_ptr<Request>(new Request(res, &req));
			res->getHttpSocket()->setUserData(rs);
		} else {
			(*rs)->updateData(res, &req);
		}

		nlohmann::json j;

		if (len != 0) {
			j = nlohmann::json::parse(data, data + len, nullptr, false);
		}

		auto args(tokenize(req.getUrl().toString(), '/', true));

		try {
			urldecode(args);
		} catch (const std::exception& e) {
			(*rs)->writeStatus("400 Bad Request");
			(*rs)->end({
				{"reason", e.what()}
			});

			(*rs)->invalidateData();
			return;
		}

		exec(*rs, std::move(j), std::move(args));
		(*rs)->invalidateData();
	});

	h.onCancelledHttpRequest([] (uWS::HttpResponse * res) {
		// requests only get cancelled when the requester disconnects, right?
		if (auto * rs = static_cast<ll::shared_ptr<Request> *>(res->getHttpSocket()->getUserData())) {
			Request& rref = **rs;
			rref.cancel(std::move(*rs));
			delete rs;
			res->getHttpSocket()->setUserData(nullptr);
		}
	});

	h.onHttpDisconnection([] (uWS::HttpSocket<uWS::SERVER> * s) {
		// this library is retarded so the disconnection handler is called
		// before the cancelled requests handler, so i need to do hacky things
		// if i don't want to use freed memory (there's no request completion handler), cool!
		ll::shared_ptr<Request> * rs = static_cast<ll::shared_ptr<Request> *>(s->getUserData());
		if (!s->outstandingResponsesHead) {
			// seems like no cancelled request handler will be called
			delete rs; // note: deleting null is ok
		}
	});
}

TemplatedEndpointBuilder ApiProcessor::on(ApiProcessor::Method m, ApiProcessor::AccessRules ar) {
	return TemplatedEndpointBuilder(*this, m, ar);
}

void ApiProcessor::add(ApiProcessor::Method m, std::unique_ptr<Endpoint> ep) {
	definedEndpoints[m].emplace_back(std::move(ep));
}

void ApiProcessor::exec(ll::shared_ptr<Request> r, nlohmann::json j, std::vector<std::string> args) {
	int m = r->getData()->getMethod(); // lol

	if (m == ApiProcessor::Method::INVALID) {
		r->writeStatus("400 Bad Request");
		r->end();
		return;
	}

	Session * sess = getSession(*r.get());

	for (auto& ep : definedEndpoints[m]) {
		if (ep->verify(args)) {
			Request& rref = *r.get();
			try {
				if (sess) {
					ep->exec(std::move(r), std::move(j), *sess, std::move(args));
				} else {
					ep->exec(std::move(r), std::move(j), std::move(args));
				}
			} catch (const std::exception& e) {
				// The request wasn't freed yet. (1 ref left in socket userdata)
				rref.writeStatus("400 Bad Request");
				rref.end({
					{"reason", e.what()}
				});
			}
			return;
		}
	}

	r->writeStatus("501 Not Implemented");
	//r->writeStatus("404 Not Found");
	r->end();
}

Session * ApiProcessor::getSession(Request& r) {
	uWS::HttpRequest& req = *r.getData();
	if (uWS::Header auth = req.getHeader("Authorization", 13)) {
		std::string b64tok(auth.toString()); // change to string_view
		sz_t i = b64tok.find("owop ");
		if (i == std::string::npos) {
			return nullptr;
		}

		i += 5;

		return am.getSession(b64tok.c_str() + i, b64tok.size() - i);
	}

	return nullptr;
}



Request::Request(uWS::HttpResponse * res, uWS::HttpRequest * req)
: cancelHandler(nullptr),
  res(res),
  req(req) { }

uWS::HttpResponse * Request::getResponse() {
	return res;
}

uWS::HttpRequest * Request::getData() {
	return req;
}

void Request::writeStatus(std::string s) {
	writeStatus(s.data(), s.size());
}

void Request::writeStatus(const char * b, sz_t s) {
	write("HTTP/1.1 ", 9);
	write(b, s);
	write("\r\n", 2);
}

void Request::writeHeader(std::string key, std::string value) {
	write(key.data(), key.size());
	write(": ", 2);
	write(value.data(), value.size());
	write("\r\n", 2);
}

void Request::end(const char * buf, sz_t size) {
	if (bufferedData.size()) {
		writeHeader("Content-Length", std::to_string(size));
		write("\r\n", 2);
	}

	writeAndEnd(buf, size);
}

void Request::end(nlohmann::json j) {
	std::string s(j.dump());
	if (!bufferedData.size()) {
		writeStatus("200 OK", 6);
	}

	writeHeader("Content-Type", "application/json");
	end(s.data(), s.size());
}

void Request::end() {
	if (!bufferedData.size()) {
		res->end();
	} else {
		writeAndEnd("Content-Length: 0\r\n\r\n", 21);
	}
}

bool Request::isCancelled() const {
	return res == nullptr;
}

void Request::onCancel(std::function<void(ll::shared_ptr<Request>)> f) {
	cancelHandler = std::move(f);
}

void Request::write(const char * b, sz_t s) {
	bufferedData.append(b, s);
}

void Request::writeAndEnd(const char * b, sz_t s) {
	if (!bufferedData.size()) {
		res->end(b, s);
	} else {
		res->hasHead = true;
		bufferedData.append(b, s);
		res->end(bufferedData.data(), bufferedData.size());
	}
}

void Request::cancel(ll::shared_ptr<Request> r) {
	// careful, this could be the last reference
	res = nullptr;
	if (cancelHandler) {
		cancelHandler(std::move(r));
	}
}

void Request::updateData(uWS::HttpResponse * res, uWS::HttpRequest * req) {
	this->res = res;
	this->req = req;
	cancelHandler = nullptr;
	bufferedData.clear();
}

void Request::invalidateData() {
	req = nullptr;
}



TemplatedEndpointBuilder::TemplatedEndpointBuilder(ApiProcessor& tc, ApiProcessor::Method m, ApiProcessor::AccessRules ar)
: targetClass(tc),
  method(m),
  ar(ar) { }

TemplatedEndpointBuilder& TemplatedEndpointBuilder::path(std::string s) {
	if (s.size() == 0) {
		throw std::runtime_error("Path sections can't be empty!");
	}

	varMarkers.emplace_back(std::move(s));
	return *this;
}

TemplatedEndpointBuilder& TemplatedEndpointBuilder::var() {
	varMarkers.emplace_back("");
	return *this;
}

Endpoint::Endpoint(ApiProcessor::AccessRules ar)
: ar(ar) { }

Endpoint::~Endpoint() { }

ApiProcessor::AccessRules Endpoint::getRules() const {
	return ar;
}

void Endpoint::exec(ll::shared_ptr<Request> req, nlohmann::json j, std::vector<std::string> arg) {
	// If this method isn't implemented, then the other must be, right?
	// This means that the request wasn't authenticated, or the token is invalid.
	req->writeStatus("401 Unauthorized");
	req->end();
}

void Endpoint::exec(ll::shared_ptr<Request> req, nlohmann::json j, Session&, std::vector<std::string> arg) {
	// If this endpoint doesn't have the authorized overload implemented, call the other method
	exec(std::move(req), std::move(j), std::move(arg));
}