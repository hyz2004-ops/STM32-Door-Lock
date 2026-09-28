// 盯梢脚本: 订阅 doorlockLog, 10 秒后向 doorlock002 发 "on", 打印 40 秒内所有消息
const WebSocketUrl = "ws://127.0.0.1:9001/mqtt";

function mqttConnect(cid) {
  const vh = Buffer.concat([Buffer.from([0,4]), Buffer.from("MQTT"), Buffer.from([4,0x02,0,60])]);
  const pl = Buffer.concat([Buffer.from([0,cid.length]), cid]);
  return Buffer.concat([Buffer.from([0x10, vh.length+pl.length]), vh, pl]);
}
function mqttSub(t){const b=Buffer.from(t);const p=Buffer.concat([Buffer.from([0,1,b.length>>8,b.length&255]),b,Buffer.from([0])]);return Buffer.concat([Buffer.from([0x82,p.length]),p]);}
function mqttPub(t,m){const b=Buffer.from(t),c=Buffer.from(m);const p=Buffer.concat([Buffer.from([b.length>>8,b.length&255]),b,c]);return Buffer.concat([Buffer.from([0x30,p.length]),p]);}

const ws = new WebSocket(WebSocketUrl, "mqtt");
ws.binaryType = "arraybuffer";
ws.onopen = () => { ws.send(mqttConnect(Buffer.from("watcher-" + Date.now().toString(36)))); };
ws.onmessage = e => {
  const d = Buffer.from(e.data);
  if (d[0] >> 4 === 2) {
    console.log("[*] 已连接代理，订阅 doorlockLog");
    ws.send(mqttSub("doorlockLog"));
    setTimeout(() => { ws.send(mqttPub("doorlock002", "on")); console.log("[>] 已发送远程开锁指令 on -> doorlock002"); }, 10000);
  } else if (d[0] >> 4 === 3) {
    const tl = d.readUInt16BE(2);
    console.log("[<] 门锁上报 [" + d.slice(4,4+tl).toString() + "] " + d.slice(4+tl).toString());
  }
};
setTimeout(() => { console.log("[*] 观察结束"); process.exit(0); }, 40000);
