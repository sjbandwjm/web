#include "chat_server/room.h"
#include "common/rapidjson_wrapper.h"
#include "butil/time.h"
namespace jiayou::chat {
void Room::Init(const RoomWorkerContext& ctx) {
	LOG(INFO) << "room init room_id:" <<  ctx.room_id
				<< " passwd:" << ctx.passwd;
	room_id_ = ctx.room_id;
	passwd_ = ctx.passwd;
	status_ = Status::kNormal;
}

bool Room::AddUser(const RoomWorkerContext& ctx) {
	auto hdl = ctx.ctx.hdl.lock();
	if (!hdl) {
		LOG(WARNING) << "room add user lock fail room_id:" << room_id_;
		return false;
	}

	if (!passwd_.empty() && passwd_ != ctx.passwd) {
		LOG(WARNING) << "room add user fail room_id:" << room_id_;
		return false;
	}

	LOG(INFO) << "room add user, room_id:" << room_id_
				<< " user_name:" << hdl->data.user_name;

	auto& user = users_[ctx.from_user_id];
	if (!user.expired()) {
		return false;
	}

	user = hdl;
	return true;
}

void Room::DelUser(const RoomWorkerContext& ctx) {
	users_.erase(ctx.from_user_id);
}


bool Room::IsUserEnter(const std::string& user_id) {
	// avoid repeated enter
	if (auto it = users_.find(user_id); it != users_.end()) {
		return true;
	}

	return false;
}

void Room::SendToAll(std::string_view msg) {
	for (auto& item : users_) {
		Server::Send(item.second, msg);
	}
	LOG(INFO) << "room send to all room_id:" << room_id_
						<< " message:" << msg;
}

void Room::SendUserList(Server::ConnectionHdl hdl) {
	base::JsonStringWriter res;
	res["cmd"] = "recv_user_list";
	res["from_room_id"] = room_id_;
	res["users"].SetArray([this](base::JsonStringWriter& arr){
		for (auto& item : users_) {
			arr.Append(item.first);
		}
	});

	Server::Send(hdl, res.GetStringView());
}

Room::Status Room::CheckStatus() {
	for (auto user : users_) {
		if (user.second.expired()) {
			users_.erase(user.first);
		}
	}

	switch (status_) {
		case Status::kWaitDel:
			status_.store(users_.empty() ? Status::kDeleted : Status::kNormal);
			break;
		case Status::kNormal:
			status_.store(users_.empty() ? Status::kWaitDel : Status::kNormal);
			break;
		default:
			LOG(ERROR) << "unexpected status:" << (int)status_.load()
									<< " room_id:" << room_id_;
			break;
	}

	return status_;
}




}