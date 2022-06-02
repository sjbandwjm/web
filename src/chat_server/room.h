#pragma once
#include <memory>
#include "chat_server/connection.h"
namespace jiayou::chat {

class Room;
typedef std::unique_ptr<Room> RoomUPtr;
typedef std::shared_ptr<Room> RoomPtr;
struct RoomWorkerContext {
	std::string room_id;
	std::string passwd;
	std::string from_name;
	std::string from_user_id;
	std::string data;
	ConnectionCtx ctx;
	std::function<void(const RoomPtr&, RoomWorkerContext*)> fn;
	void Reset() {};
};

class Room {
 public:
	enum class Status {
		kUndefined = 0,
		kNormal,
		kWaitDel,
		kDeleted
	};

	void Init(const RoomWorkerContext&);
	bool IsUserEnter(const std::string& user_id);
	bool AddUser(const RoomWorkerContext&);
	void DelUser(const RoomWorkerContext&);
	void SendToAll(std::string_view msg);
	void SendUserList(Server::ConnectionHdl);
	Status CheckStatus();
	Status GetStatus() { return status_.load(); }
	void SetStatus(Status stat) { status_.store(stat); }

	const std::map<std::string, Server::ConnectionHdl>& users() { return users_; }
	const std::string room_id() {
		return room_id_;
	}
 private:
	std::atomic<Status> status_{Status::kUndefined};
	std::map<std::string, Server::ConnectionHdl> users_;
	std::string room_id_;
	std::string passwd_;
};




}