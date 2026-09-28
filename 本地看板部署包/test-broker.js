// 验证 Mosquitto: ws://9001 订阅/发布 与 tcp://1883 互通
const { execFileSync, spawn } = require("child_process");
const MOSQ = "C:\\Program Files\\mosquitto\\";

function mqttConnect(cid) {
  const cidB = Buffer.from(cid);
  const vh = Buffer.concat([Buffer.from([0,4]), Buffer.from("MQTT"),
    Buffer.from([4, 0x02, 0, 60])]);
  const pl = Buffer.concat([Buffer.from([0, cidB.length]), cidB]);
  const rl = vh.length + pl.length;
  return Buffer.concat([Buffer.from([0x10, rl]), vh, pl]);
}
function mqttSub(topic, mid = 1) {
  const t = Buffer.from(topic);
  const pl = Buffer.concat([Buffer.from([mid >> 8, mid & 255, t.length >> 8, t.length & 255]), t, Buffer.from([0])]);
  return Buffer.concat([Buffer.from([0x82, pl.length]), pl]);
}
function mqttPub(topic, msg) {
  const t = Buffer.from(topic), m = Buffer.from(msg);
  const pl = Buffer.concat([Buffer.from([t.length >> 8, t.length & 255]), t, m]);
  return Buffer.concat([Buffer.from([0x30, pl.length]), pl]);
}

(async () => {
  const ws = new WebSocket("ws://127.0.0.1:9001/mqtt", "mqtt");
  ws.binaryType = "arraybuffer";
  const open = new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  await open;
  console.log("[1] WS 握手 OK");
  ws.send(mqttConnect("test-node-dashboard"));
  const connack = await new Promise(res => ws.onmessage = e => res(Buffer.from(e.data)));
  if (connack[0] >> 4 !== 2 || connack[2] !== 0) throw new Error("CONNACK 异常 " + connack.toString("hex"));
  console.log("[2] MQTT CONNECT -> CONNACK OK");

  ws.send(mqttSub("doorlockLog"));
  await new Promise(r => setTimeout(r, 300));
  execFileSync(MOSQ + "mosquitto_pub.exe", ["-h","127.0.0.1","-p","1883","-t","doorlockLog","-m","open,CARD,id2,no7,14:32:10"]);
  const pub = await new Promise((res, rej) => {
    const to = setTimeout(() => rej(new Error("超时未收到")), 5000);
    ws.onmessage = e => { clearTimeout(to); res(Buffer.from(e.data)); };
  });
  const tl = pub.readUInt16BE(2);
  const topic = pub.slice(4, 4 + tl).toString();
  const body = pub.slice(4 + tl).toString();
  console.log(`[3] TCP 1883 -> WS 收到 [${topic}] ${body}`);
  if (topic !== "doorlockLog" || !body.startsWith("open,CARD")) throw new Error("内容不对");

  const sub = spawn(MOSQ + "mosquitto_sub.exe", ["-h","127.0.0.1","-p","1883","-t","doorlock002","-C","1","-W","8"]);
  await new Promise(r => setTimeout(r, 500));
  ws.send(mqttPub("doorlock002", "on"));
  const got = await new Promise(res => sub.stdout.once("data", res));
  console.log("[4] WS 发布 -> TCP 侧收到:", got.toString().trim());
  if (got.toString().trim() !== "on") throw new Error("TCP 侧内容不对");

  ws.close();
  console.log("\n结果: 全部通过 ✅");
  process.exit(0);
})().catch(e => { console.error("结果: 失败 ✘", e.message); process.exit(1); });
