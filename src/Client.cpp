#include "Client.hpp"

#include <stdexcept>

#include <uWS.h>

#include <utils.hpp>
#include <PrepMsg.hpp>
#include <shared_ptr_ll.hpp>

#include <World.hpp>
#include <Session.hpp>

Client::Client(uWS::WebSocket<uWS::SERVER> * ws, ll::shared_ptr<Session> s, Ip ip, Player::Builder& pb)
: ws(ws),
  session(std::move(s)),
  connectedOn(std::chrono::steady_clock::now()),
  lastAction(std::chrono::steady_clock::now()),
  ip(ip),
  pl(pb.setClient(*this)) {
  	if (!session) {
  		throw std::invalid_argument("Client session is null?!");
  	}

	session->addClient(*this);
}

Client::~Client() {
	session->delClient(*this);
}

void Client::updateLastActionTime() {
	lastAction = std::chrono::steady_clock::now();
}

bool Client::inactiveKickEnabled() const {
	return true;
}

std::chrono::seconds Client::getSecondsConnected() const {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - connectedOn);
}

std::chrono::steady_clock::time_point Client::getLastActionTime() const {
	return lastAction;
}

Ip Client::getIp() const {
	return ip;
}

uWS::WebSocket<true> * Client::getWs() {
	return ws;
}

Player& Client::getPlayer() {
	return pl;
}

Session& Client::getSession() {
	return *session;
}

User& Client::getUser() {
	return session->getUser();
}

void Client::send(const PrepMsg& p) {
	ws->sendPrepared(static_cast<uWS::WebSocket<uWS::SERVER>::PreparedMessage *>(p.getPrepared()));
}

void Client::close() {
	ws->close();
}

bool Client::operator ==(const Client& c) const {
	// Client objects are NOT meant to be copied
	return this == std::addressof(c);
}
