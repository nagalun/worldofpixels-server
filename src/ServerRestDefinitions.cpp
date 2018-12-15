#include "Server.hpp"

#include <iostream>

#include <WorldManager.hpp>
#include <User.hpp>
#include <Player.hpp>
#include <World.hpp>

#include <misc/shared_ptr_ll.hpp>
#include <misc/utils.hpp>
#include <misc/base64.hpp>

#include <nlohmann/json.hpp>

void Server::registerEndpoints() {
	/*api.on(ApiProcessor::GET)
		.path("get")
		.var()
	.onOutsider(true, [this] (ll::shared_ptr<Request> req, nlohmann::json, std::string url) {
		hcli.addRequest(std::move(url), [req{std::move(req)}] (auto result) {
			std::cout << "done: " << result.successful << " (" << result.responseCode << "): ";
			if (result.errorString) std::cout << result.errorString;
			if (req->isCancelled()) {
				std::cout << " [Request was cancelled!!]" << std::endl;
				return;
			}

			std::cout << std::endl;
			req->end(result.data.c_str(), result.data.size());
		});
	});*/

	api.on(ApiProcessor::GET)
		.path("auth")
		.path("guest")
	.onOutsider(true, [this] (ll::shared_ptr<Request> req, nlohmann::json) {
		std::string ua;
		std::string lang("en");

		if (auto h = req->getData()->getHeader("accept-language", 15)) {
			auto langs(tokenize(h.toString(), ',', true));
			if (langs.size() != 0) {
				lang = std::move(langs[0]);
				// example: "en-US;q=0.9", we want just "en" (for now)
				sz_t i = lang.find_first_of("-;");
				if (i != std::string::npos) {
					lang.erase(i); // from - or ; to the end
				}
			}
		}

		if (auto h = req->getData()->getHeader("user-agent", 10)) {
			ua = h.toString();
		}

		Ipv4 ip(req->getIp()); // done here because the move invalidates ref before calling getIp
		am.createGuestSession(ip, std::move(ua), std::move(lang), [req{std::move(req)}] (auto token, Session& s) {
			if (!req->isCancelled()) {
				std::string b64tok(base64Encode(token.data(), token.size()));
				req->end({
					{ "token", std::move(b64tok) }
				});
			} else {
				// expire the session?
			}
		});
	});

	//////////////////// /users

	// Get local user
	api.on(ApiProcessor::GET) // definition order matters here!
		.path("users")
		.path("@me")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s) {
		req->end(s.getUser());
	});

	// Get user
	api.on(ApiProcessor::GET)
		.path("users")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, User::Id uid) {
		am.getOrLoadUser(uid, [req{std::move(req)}] (ll::shared_ptr<User> user) {
			if (!req->isCancelled()) {
				req->end(*user);
			}
		});
	});

	// Modify user
	api.on(ApiProcessor::PATCH)
		.path("users")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, User::Id uid) {
		/*req->writeStatus("405 Method Not Allowed");
		req->end();*/
	});

	// Kick user
	api.on(ApiProcessor::POST)
		.path("users")
		.var()
		.path("kick")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, User::Id uid) {

	});

	// Ban user
	api.on(ApiProcessor::POST)
		.path("users")
		.var()
		.path("ban")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, User::Id uid) {

	});

	/////////////////////// /worlds

	api.on(ApiProcessor::GET) // Get world
		.path("worlds")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName) {
		if (wm.isLoaded(worldName)) {
			req->end(wm.getOrLoadWorld(worldName));
		} else {
			req->writeStatus("404 Not Found");
			req->end();
		}
	});

	api.on(ApiProcessor::PATCH) // Modify world
		.path("worlds")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName) {

	});

	api.on(ApiProcessor::GET) // Switch world
		.path("worlds")
		.var()
		.path("cursors")
		.var()
		.path("move")
		.path("world")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, Player::Id pid, std::string newWorldName) {

	});

	api.on(ApiProcessor::GET) // Teleport to pos
		.path("worlds")
		.var()
		.path("cursors")
		.var()
		.path("move")
		.path("pos")
		.var()
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, Player::Id pid, World::Pos x, World::Pos y) {

	});

	api.on(ApiProcessor::GET) // Teleport to player
		.path("worlds")
		.var()
		.path("cursors")
		.var()
		.path("move")
		.path("player")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, Player::Id fromPid, Player::Id toPid) {

	});

	api.on(ApiProcessor::GET) // Kick own player
		.path("worlds")
		.var()
		.path("cursors")
		.var()
		.path("kick")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, Player::Id pid) {

	});

	api.on(ApiProcessor::GET) // Get world roles
		.path("worlds")
		.var()
		.path("roles")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName) {

	});

	api.on(ApiProcessor::POST) // Create world role
		.path("worlds")
		.var()
		.path("roles")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName) {

	});

	api.on(ApiProcessor::PATCH) // Modify world role
		.path("worlds")
		.var()
		.path("roles")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, std::string roleId) {

	});

	api.on(ApiProcessor::DELETE) // Delete world role
		.path("worlds")
		.var()
		.path("roles")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, std::string roleId) {

	});

	api.on(ApiProcessor::PUT) // Add user to world role
		.path("worlds")
		.var()
		.path("roles")
		.var()
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, std::string roleId, User::Id uid) {

	});

	api.on(ApiProcessor::DELETE) // Delete user from world role
		.path("worlds")
		.var()
		.path("roles")
		.var()
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, std::string roleId, User::Id uid) {

	});

	/////////////////////// /chats

	api.on(ApiProcessor::GET) // Get chat
		.path("chats")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string chatId) {

	});

	api.on(ApiProcessor::GET) // Get messages
		.path("chats")
		.var()
		.path("messages")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string chatId) {

	});

	api.on(ApiProcessor::POST) // Send message
		.path("chats")
		.var()
		.path("messages")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string chatId) {

	});

	api.on(ApiProcessor::GET) // Get message
		.path("chats")
		.var()
		.path("messages")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string chatId, std::string messageId) {

	});

	api.on(ApiProcessor::PATCH) // Edit message
		.path("chats")
		.var()
		.path("messages")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string chatId, std::string messageId) {

	});

	api.on(ApiProcessor::DELETE) // Delete message
		.path("chats")
		.var()
		.path("messages")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string chatId, std::string messageId) {

	});

	/////////////////////// /emotes

	api.on(ApiProcessor::GET)
		.path("emotes")
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string emoteId) {

	});

	api.on(ApiProcessor::GET)
		.path("emotes")
		.var()
		.path("image")
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string emoteId) {

	});

	///////////////////////
	// Extras
	///////////////////////

	api.on(ApiProcessor::GET)
		.path("status")
	.onOutsider(false, [this] (ll::shared_ptr<Request> req, nlohmann::json) {
		Ipv4 ip(req->getIp());

		bool banned = bm.isBanned(ip);

		nlohmann::json j = {
			{ "motd", "Almost done!" },
			{ "activeHttpHandles", hcli.activeHandles() },
			{ "uptime", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startupTime).count() }, // lol
			{ "yourIp", ip },
			{ "banned", banned },
			{ "tps", wm.getTps() }
		};

		nlohmann::json processorInfo;

		conn.forEachProcessor([&processorInfo] (ConnectionProcessor& p) {
			nlohmann::json j = p.getPublicInfo();
			if (!j.is_null()) {
				processorInfo[demangle(typeid(p))] = std::move(j);
			}
		});

		j["connectInfo"] = std::move(processorInfo);

		if (banned) {
			j["banInfo"] = bm.getInfoFor(ip);
		}

		req->end(j);
	});

	api.on(ApiProcessor::GET)
		.path("view")
		.var()
		.var()
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, i32 x, i32 y) {
		//std::cout << "[" << j << "]" << worldName << ","<< x << "," << y << std::endl;
		if (!wm.verifyWorldName(worldName)) {
			req->writeStatus("400 Bad Request", 15);
			req->end();
			return;
		}

		if (!wm.isLoaded(worldName)) {
			// nginx is supposed to serve unloaded worlds and chunks.
			req->writeStatus("404 Not Found", 13);
			req->end();
			return;
		}

		World& world = wm.getOrLoadWorld(worldName);

		// will encode the png in another thread if necessary and end the request when done
		world.sendChunk(x, y, std::move(req));
	});

	api.on(ApiProcessor::GET)
		.path("tokenlist")
	.onOutsider(false, [this] (ll::shared_ptr<Request> req, nlohmann::json) {
		nlohmann::json j;
		am.forEachSession([&j] (const auto& token, const auto& session) {
			std::string b64tok(base64Encode(token.data(), token.size()));
			j[b64tok] = session;
		});

		req->end(std::move(j));
	});

	api.on(ApiProcessor::GET)
		.path("debug")
	.onAny([] (ll::shared_ptr<Request> req, nlohmann::json j) {

		req->end({
			{ "call", "outsider" },
			{ "body", std::move(j) }
		});

	}, [] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s) {

		req->end({
			{ "call", "friend" },
			{ "body", std::move(j) },
			{ "session", s }
		});

	});
}