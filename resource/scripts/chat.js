// buttom
const sendmsg = document.querySelector("#sendmsg");
const enter_room = document.querySelector("#enter_room");
const login = document.querySelector("#login");
const create_rom = document.querySelector("#create_room");

// div
const input_div = document.querySelector("#input_msg");
let recv_msg_div = document.querySelector('#recvmsg');
let user_name_play_div = document.querySelector("#user_name_show");
let room_list_div = document.querySelector("#room_list")

let cur_select_room = {
    "room_id": "",
    "output_div" : recv_msg_div,
    "user_list_div":null,
    "room_div":null
}


let rooms = {
}

let operation_id = 0;

let websock;

input_div.addEventListener("click", function(e) {
    input_div.value = "";
});


// send msg
sendmsg.addEventListener("click", function(e) {
    if (cur_select_room.room_id.length === 0) {
        return
    }

    let msg = input_div.value;
    let req = {
        "cmd":"send_message",
        "to_room_id": cur_select_room.room_id,
        "message": String(msg)
    }
    console.log("send: " + JSON.stringify(req) + operation_id++);
    websock.send(JSON.stringify(req));
    input_div.value = "";
});

// enter_room
enter_room.addEventListener("click", function(e){
    let room_id = prompt("请输入房间ID:");
    if (!room_id) return;

    let passwd = prompt("请输入房间密码：");

    let req = {
        "cmd":"enter_room",
        "room_id":String(room_id),
        "passwd":String(passwd)
    }
    console.log("send: " + JSON.stringify(req) + operation_id++);
    websock.send(JSON.stringify(req));
})

// create_room
create_rom.addEventListener("click", function(e){
    let room_id = prompt("请输入房间ID:");
    if (!room_id) return;

    let passwd = prompt("请输入房间密码：");

    let token = prompt("请输入token:");

    let req = {
        "cmd":"create_room",
        "room_id":String(room_id),
        "passwd":String(passwd),
        "token":token
    }
    console.log("send: " + JSON.stringify(req) + operation_id++);
    websock.send(JSON.stringify(req));
})

// login
login.addEventListener("click", function(e){
    let protocol = document.location.protocol;
    let ws_url;
    if (protocol === "http:") {
        ws_url = "ws://";
    } else if (protocol === "https:") {
        ws_url = "wss://";
    }
    console.log(protocol);

    ws_url = ws_url + window.location.host;
    websock = new WebSocket(ws_url);
    InitWeb(websock);
})


//////////////////////////////////////////////////////////////////////
////

function ProcessRecvMsg(msg) {
    let json_res = null
    try {
        json_res = JSON.parse(msg);
    } catch(err) {
        let newContent = document.createElement("p");
        newContent.textContent = String(msg);
        ShowMsgToCurRoom(newContent)
        return;
    }

    const cmd_process = {
        "on_login": function() {
            const user_name = json_res["user_name"];
            const user_id = json_res["user_id"];
            user_name_play_div.textContent = "welcome, " + user_id;
        },

        "recv_message" : function() {
            const room_id = json_res["from_room"];
            var new_content = document.createElement("p");
            new_content.textContent = "[" + json_res["from_user"] + " " + json_res["timestamp"] + "]:\n" + json_res["message"];
            if (!( room_id in rooms)) {
                ShowMsgToCurRoom(new_content);
                return;
            }

            let cur = rooms[room_id].output_div;
            cur.appendChild(new_content, cur.firstChild);

            // 如果消息超过容器高度，滚动到底部
            cur.scrollTop = cur.scrollHeight - cur.clientHeight;

            if (cur.childElementCount > 100) {
                cur.removeChild(cur.firstChild)
            }
        },

        "enter_room_success": function() {
            let new_room = {};
            // room_id
            const room_id = json_res["from_room_id"]
            new_room["room_id"] = room_id;
            // output
            let output = document.createElement("div");
            output.className = "output_scroll"
            new_room["output_div"] = output;
            // user_list
            let user_list = document.createElement("div");
            user_list.className = "scroll";
            new_room.user_list_div = user_list;
            // room_id
            let room_p = document.createElement("button");
            room_p.onclick = function(e) {
                SwitchToRoom(room_id);
            };
            room_p.textContent = "room:" + room_id;
            new_room.room_div = room_p
            // add to rooms
            rooms[new_room["room_id"]] = new_room;

            // add room to room_list
            room_list_div.appendChild(room_p);
            SwitchToRoom(room_id);
        },
        "recv_user_list":function(){
            let room_id = json_res["from_room_id"];
            if (!(room_id in rooms)) {
                console.log("no such room");
                return
            }

            rooms[room_id].user_list_div.replaceChildren();
            // 去重
            if (!("users" in rooms[room_id])) {
                rooms[room_id]["users"] = {}
            }
            for (user of json_res["users"]) {
                rooms[room_id]["users"][user] = {};
            }

            for (user in rooms[room_id].users) {
                let user_p = document.createElement("p");
                user_p.textContent = user;
                rooms[room_id].user_list_div.appendChild(user_p);
            }
        },

        "on_user_enter":function() {
            const room_id = json_res["from_room_id"];
            if (!(room_id in rooms)) {
                console.log("no such room");
                return
            }

            rooms[room_id].user_list_div.replaceChildren();
            // 去重
            if (!("users" in rooms[room_id])) {
                rooms[room_id]["users"] = {}
            }

            const user_id = json_res["from_user_id"];
            rooms[room_id].users[user_id] = {}

            for (user in rooms[room_id].users) {
                let user_p = document.createElement("p");
                user_p.textContent = user;
                rooms[room_id].user_list_div.appendChild(user_p);
            }
        },

        "on_user_quit":function() {
            const room_id = json_res["from_room_id"];
            if (!(room_id in rooms)) {
                console.log("no such room");
                return
            }



            // 去重
            if (!("users" in rooms[room_id])) {
                rooms[room_id]["users"] = {}
            }

            const user_id = json_res["from_user_id"];
            if (!(user_id in rooms[room_id].users)) {
                return;
            }

            delete rooms[room_id].users[user_id];

            // reset
            rooms[room_id].user_list_div.replaceChildren();

            for (user in rooms[room_id].users) {
                let user_p = document.createElement("p");
                user_p.textContent = user;
                rooms[room_id].user_list_div.appendChild(user_p);
            }
        }
    }

    if (json_res["cmd"] in cmd_process) {
        console.log(json_res["cmd"])
        cmd_process[json_res["cmd"]]();
    } else {
        let newContent = document.createElement("p");
        newContent.textContent = String(msg);
        ShowMsgToCurRoom(newContent)
    }
}

function ShowMsgToCurRoom(new_content) {
    let cur = cur_select_room.output_div;
    cur.appendChild(new_content, recv_msg_div.firstChild);

    // 如果消息超过容器高度，滚动到底部
    cur.scrollTop = cur.scrollHeight - cur.clientHeight;

    if (cur.childElementCount > 100) {
        cur.removeChild(cur.firstChild)
    }
}

function SwitchToRoom(room_id) {
    console.log("switch to room:" + room_id)
    if (!(room_id in rooms)) {
        console.log(room_id + "not int rooms");
        return;
    }

    if (cur_select_room.user_list_div !== null) {
        room_list_div.removeChild(cur_select_room.user_list_div);
    }


    cur_select_room.room_id = room_id;
    cur_select_room.output_div = rooms[room_id].output_div;
    cur_select_room.user_list_div = rooms[room_id].user_list_div;

    console.log(cur_select_room.output_div)
    recv_msg_div.replaceChildren(cur_select_room.output_div);
    // 如果消息超过容器高度，滚动到底部
    cur_select_room.output_div.scrollTop = cur_select_room.output_div.scrollHeight - cur_select_room.output_div.clientHeight;

    room_list_div.appendChild(cur_select_room.user_list_div);
}


// websocket
function InitWeb (websock) {
    websock.onopen = function(event) {
        let user_name = prompt("请输入用户昵称：")
        if (!user_name) return;

        let req = {
            "cmd":"login",
            "user_name": String(user_name)
        }
        console.log("send: " + JSON.stringify(req) + operation_id++)
        websock.send(JSON.stringify(req));
    };

    // 监听消息接收事件
    websock.onmessage = function(event) {
        console.log("on_message: " + event.data);
        ProcessRecvMsg(String(event.data));
    };

    // 监听连接关闭事件
    websock.onclose = function(event) {
        if (event.wasClean) {
        console.log("WebSocket连接已关闭，代码:", event.code, "原因:", event.reason);
        } else {
        console.log("WebSocket连接异常断开");
        }
    };

    // 监听连接错误事件
    websock.onerror = function(event) {
        console.error("WebSocket连接错误:", event);
    };
}

