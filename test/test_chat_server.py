import asyncio
from websockets.sync.client import connect
import sys
import threading
import json


class myThread (threading.Thread):
    def __init__(self, threadID, ws):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.ws = ws
    def run(self):
        while True:
            message = self.ws.recv()
            print(f"\nrecv: {message}")



if __name__ == "__main__":
    url = "ws://" + sys.argv[1]

    with connect(url) as websocket:
        t1 = myThread("recvthread", websocket)
        t1.start()

        while True:
            cmd = input("please input cmd:")
            req = dict()
            # login
            if cmd == "login":
                req["cmd"] = "login"
                req["user_name"] = "test"
            elif cmd == "enter_room":
                req["cmd"] = "enter_room"
                req["room_id"] = input("input room_id:")
                req["passwd"] = input("input passwd:")
            elif cmd == "create_room":
                req["cmd"] = cmd
                req["room_id"] = input("input room_id:")
                req["passwd"] = input("input passwd:")
            elif cmd == "send_message":
                req["cmd"] = cmd
                req["to_room_id"] = input("input to room id:")
                req["message"] = input("input message:")
            websocket.send(json.dumps(req))


        t1.join()