#include "LayoutTemplates.h"

// Screen is 170 x 320. The root style leaves a content box of 154 px, so
// fixed widths inside a row add up to that (less a card's padding). Bitmap
// text is 6 x 8 per char times size: size 1 fits 25 columns there. Default
// config has three ping targets, so preloaded layouts show ping.0 to
// ping.2; Server Rack folds its last three rows away until they exist.
// None of them needs a data source or a sampled key, except Weather.

static const char BLANK[] = R"json({
  "name": "My Widget",
  "elements": [
    {"type":"text","x":85,"y":150,"size":2,"align":"center","color":"text","text":"Hello"}
  ]
})json";

static const char BIG_CLOCK[] = R"json({
  "name": "Big Clock",
  "led": {"color":"{wifi.color}","mode":"breathe","speed":4000,"brightness":30},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"{date.dow}","color":"#7c8794"},
      {"type":"text","text":"{wifi.bars}","align":"right","color":"{wifi.color}"}
    ]},
    {"type":"box","h":206,"style":{"padding":8,"radius":14,"background":"{hsv(215,70,8+26*clamp(1-abs(time.hour-13)/7,0,1))}","gradient":"#05070a","border":"#1c2535"},"children":[
      {"type":"arc","x":0,"y":8,"w":138,"value":"{(time.hour*60+time.min)/14.4}","start":270,"end":450,"color":"#7f9fd8","style":{"thickness":3,"background":"#2a3850"}},
      {"type":"arc","x":-3,"y":5,"w":144,"value":100,"start":"{265+(time.hour*60+time.min)/8}","end":"{275+(time.hour*60+time.min)/8}","color":"{if(abs(time.hour-12.5)<6,rgb(255,190,60),rgb(150,170,255))}","style":{"thickness":9}},
      {"type":"text","x":69,"y":46,"text":"{time}","font":"bold","size":52,"align":"center"},
      {"type":"text","x":69,"y":102,"text":"{time.ampm}","align":"center","color":"#c4ccd6"},
      {"type":"line","x":30,"y":124,"x2":108,"y2":124,"color":"#2a3850"},
      {"type":"text","x":69,"y":138,"text":"{date.md}","font":"bold","size":28,"align":"center"},
      {"type":"text","x":69,"y":172,"text":"{date.year}","align":"center","color":"#c4ccd6"}
    ]},
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"DAY","color":"#7c8794"},
      {"type":"text","text":"{round((time.hour*60+time.min)/14.4)}%","align":"right"}
    ]},
    {"type":"bar","h":10,"value":"{(time.hour*60+time.min)/14.4}","color":"#1d4ed8","style":{"gradient":"#7fb4ff","gradientDir":"right","radius":5,"background":"#1a2029"}},
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"to midnight","color":"#7c8794"},
      {"type":"text","text":"{1440-(time.hour*60+time.min)} min","align":"right"}
    ]},
    {"type":"text","x":77,"y":296,"text":"{wifi.ssid}","align":"center","color":"dim"}
  ]
})json";

static const char STACKED_CLOCK[] = R"json({
  "name": "Stacked Clock",
  "led": {"color":"accent","mode":"breathe","speed":5000,"brightness":25},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","h":232,"style":{"padding":8,"radius":14,"background":"{hsv(215,70,8+26*clamp(1-abs(time.hour-13)/7,0,1))}","gradient":"#05070a","border":"#1c2535"},"children":[
      {"type":"text","x":69,"y":4,"text":"{time.hour}","font":"bold","size":104,"align":"center"},
      {"type":"text","x":69,"y":108,"text":"{time.min}","font":"bold","size":104,"align":"center","color":"accent"},
      {"type":"rect","x":20,"y":110,"w":98,"h":1,"color":"#2a3850","fill":true}
    ]},
    {"type":"text","text":"{time.ampm}","align":"center","color":"#c4ccd6"},
    {"type":"text","text":"{date.day} {date.md}","font":"sans","size":20,"align":"center"},
    {"type":"text","x":77,"y":296,"text":"{wifi.ssid}","align":"center","color":"dim"}
  ]
})json";

static const char NIGHT_CLOCK[] = R"json({
  "name": "Night Clock",
  "led": {"mode":"off"},
  "style": {"direction":"column","padding":8,"justify":"center"},
  "elements": [
    {"type":"box","w":154,"h":154,"children":[
      {"type":"arc","x":2,"y":2,"w":150,"value":"{time.min/0.6}","color":"#5a3608","style":{"thickness":2,"background":"#1c1003"}},
      {"type":"arc","x":0,"y":0,"w":154,"value":100,"start":"{time.hour%12*30+time.min/2-4}","end":"{time.hour%12*30+time.min/2+4}","color":"#a8660f","style":{"thickness":6}},
      {"type":"text","x":77,"y":50,"text":"{time}","font":"bold","size":46,"align":"center","color":"#7a4a0c"},
      {"type":"text","x":77,"y":104,"text":"{date.day} {date.md}","align":"center","color":"#4a2d07"}
    ]}
  ]
})json";

static const char DATE_CARD[] = R"json({
  "name": "Date Card",
  "led": {"color":"#ff7800","mode":"solid","brightness":20},
  "style": {"direction":"column","gap":4,"padding":8},
  "elements": [
    {"type":"box","style":{"padding":10,"radius":12,"background":"#ff453a","gradient":"#9c1610"},"children":[
      {"type":"text","text":"{date.dow}","font":"bold","size":24,"align":"center"},
      {"type":"circle","x":24,"y":-6,"r":4,"color":"bg"},
      {"type":"circle","x":102,"y":-6,"r":4,"color":"bg"}
    ]},
    {"type":"box","style":{"padding":12,"gap":2,"radius":12,"background":"#1f2631","gradient":"#0d1015","border":"#343e4e"},"children":[
      {"type":"text","text":"{date.md}","font":"bold","size":42,"align":"center"},
      {"type":"text","text":"{date.year}","font":"sans","size":18,"align":"center","color":"#7c8794"}
    ]},
    {"type":"box","style":{"gap":6,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10,"padding":8},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"NOW","color":"#7c8794"},
        {"type":"text","text":"{date}","align":"right","color":"#7c8794"}
      ]},
      {"type":"text","text":"{time}","font":"bold","size":40,"align":"center"},
      {"type":"bar","h":8,"value":"{(time.hour*60+time.min)/14.4}","color":"#9c1610","style":{"gradient":"#ff6a5e","gradientDir":"right","radius":4,"background":"#1a2029"}},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"day","color":"#7c8794"},
        {"type":"text","text":"{round((time.hour*60+time.min)/14.4)}%","align":"right","color":"#c4ccd6"}
      ]}
    ]},
    {"type":"text","x":77,"y":296,"text":"{wifi.ssid}","align":"center","color":"dim"}
  ]
})json";

static const char PING_BOARD[] = R"json({
  "name": "Ping Board",
  "led": {"color":"{ping.0.color}","mode":"solid","rules":[{"key":"ping.0.status","is":"down","mode":"blink","speed":400},{"key":"ping.1.status","is":"down","color":"bad","mode":"blink","speed":400},{"key":"ping.2.status","is":"down","color":"bad","mode":"blink","speed":400}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"{wifi.ssid}","color":"#c4ccd6"},
      {"type":"text","text":"{wifi.bars}","align":"right","color":"{wifi.color}"}
    ]},
    {"type":"box","style":{"gap":6,"padding":7,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","r":5,"color":"{ping.0.color}"},
        {"type":"text","w":84,"text":"{ping.0.name}","font":"sans","size":15},
        {"type":"text","w":36,"text":"{ping.0.ms}","font":"bold","size":16,"align":"right","color":"{ping.0.color}"}
      ]},
      {"type":"box","h":4,"children":[
        {"type":"rect","x":0,"y":0,"w":138,"h":4,"color":"#1a2029","fill":true,"style":{"radius":2}},
        {"type":"rect","x":0,"y":0,"w":"{clamp(ping.0.ms*1.38,3,138)}","h":4,"color":"{hsv(clamp(130-ping.0.ms*1.3,0,130),85,95)}","fill":true,"style":{"radius":2}}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"{ping.0.bars}","color":"{ping.0.color}"},
        {"type":"text","text":"ms {ping.0.trend}","align":"right","color":"#7c8794"}
      ]}
    ]},
    {"type":"box","style":{"gap":6,"padding":7,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","r":5,"color":"{ping.1.color}"},
        {"type":"text","w":84,"text":"{ping.1.name}","font":"sans","size":15},
        {"type":"text","w":36,"text":"{ping.1.ms}","font":"bold","size":16,"align":"right","color":"{ping.1.color}"}
      ]},
      {"type":"box","h":4,"children":[
        {"type":"rect","x":0,"y":0,"w":138,"h":4,"color":"#1a2029","fill":true,"style":{"radius":2}},
        {"type":"rect","x":0,"y":0,"w":"{clamp(ping.1.ms*1.38,3,138)}","h":4,"color":"{hsv(clamp(130-ping.1.ms*1.3,0,130),85,95)}","fill":true,"style":{"radius":2}}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"{ping.1.bars}","color":"{ping.1.color}"},
        {"type":"text","text":"ms {ping.1.trend}","align":"right","color":"#7c8794"}
      ]}
    ]},
    {"type":"box","style":{"gap":6,"padding":7,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","r":5,"color":"{ping.2.color}"},
        {"type":"text","w":84,"text":"{ping.2.name}","font":"sans","size":15},
        {"type":"text","w":36,"text":"{ping.2.ms}","font":"bold","size":16,"align":"right","color":"{ping.2.color}"}
      ]},
      {"type":"box","h":4,"children":[
        {"type":"rect","x":0,"y":0,"w":138,"h":4,"color":"#1a2029","fill":true,"style":{"radius":2}},
        {"type":"rect","x":0,"y":0,"w":"{clamp(ping.2.ms*1.38,3,138)}","h":4,"color":"{hsv(clamp(130-ping.2.ms*1.3,0,130),85,95)}","fill":true,"style":{"radius":2}}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"{ping.2.bars}","color":"{ping.2.color}"},
        {"type":"text","text":"ms {ping.2.trend}","align":"right","color":"#7c8794"}
      ]}
    ]},
    {"type":"box","style":{"gap":8,"radius":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","padding":8},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"IP","color":"#7c8794"},
        {"type":"text","text":"{wifi.ip}","align":"right"}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"uptime","color":"#7c8794"},
        {"type":"text","text":"{uptime}","align":"right"}
      ]}
    ]},
    {"type":"text","x":77,"y":296,"text":"{time}  {date.day} {date.md}","align":"center","color":"dim"}
  ]
})json";

static const char LATENCY_HERO[] = R"json({
  "name": "Latency",
  "led": {"color":"{ping.0.color}","mode":"solid","rules":[{"key":"ping.0.status","is":"down","mode":"blink","speed":300},{"when":"ping.0.ms > 100","mode":"pulse","speed":1000}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"LATENCY","color":"#7c8794"},
      {"type":"text","text":"{ping.0.status}","align":"right","color":"{ping.0.color}"}
    ]},
    {"type":"box","style":{"gap":0,"align":"center","padding":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","w":130,"h":114,"children":[
        {"type":"arc","x":0,"y":0,"w":130,"value":"{min(ping.0.ms,100)}","start":225,"end":495,"color":"{hsv(clamp(130-ping.0.ms*1.3,0,130),85,95)}","style":{"thickness":12,"background":"#1a2029"}},
        {"type":"text","x":65,"y":34,"text":"{ping.0.ms}","font":"bold","size":44,"align":"center"},
        {"type":"text","x":65,"y":80,"text":"ms","align":"center","color":"#7c8794"}
      ]},
      {"type":"text","text":"{ping.0.name}","font":"sans","size":18,"align":"center"}
    ]},
    {"type":"box","style":{"gap":6,"radius":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","padding":8},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"HISTORY","color":"#7c8794"},
        {"type":"text","text":"trend {ping.0.trend}","align":"right","color":"#c4ccd6"}
      ]},
      {"type":"text","text":"{ping.0.bars}","size":2,"align":"center","color":"{ping.0.color}"}
    ]},
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"WIFI","color":"#7c8794"},
      {"type":"text","text":"{wifi.rssi} dBm","align":"right","color":"#c4ccd6"}
    ]},
    {"type":"bar","h":8,"value":"{wifi.pct}","color":"#c0392b","style":{"gradient":"#2fd37a","gradientDir":"right","radius":4,"background":"#1a2029"}},
    {"type":"text","x":77,"y":296,"text":"up {uptime}  ·  {time}","align":"center","color":"dim"}
  ]
})json";

static const char STATUS_LIGHTS[] = R"json({
  "name": "Status Lights",
  "led": {"color":"ok","mode":"solid","rules":[{"key":"ping.0.status","is":"down","color":"bad","mode":"blink","speed":500},{"key":"ping.1.status","is":"down","color":"bad","mode":"blink","speed":500},{"key":"ping.2.status","is":"down","color":"bad","mode":"blink","speed":500},{"when":"ping.0.ms > 100 || ping.1.ms > 100 || ping.2.ms > 100","color":"warn"}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"STATUS","color":"#7c8794"},
      {"type":"text","text":"{time}","align":"right","color":"#c4ccd6"}
    ]},
    {"type":"box","style":{"direction":"row","gap":4,"align":"center","padding":7,"radius":10,"background":"#141a22","gradient":"#0b0e13","border":"{ping.0.color}"},"children":[
      {"type":"box","w":27,"h":27,"children":[
        {"type":"circle","x":0,"y":0,"r":13,"color":"#10161d","style":{"border":"{ping.0.color}","borderWidth":2}},
        {"type":"circle","x":6,"y":6,"r":7,"color":"{ping.0.color}"}
      ]},
      {"type":"box","w":70,"style":{"gap":3},"children":[
        {"type":"text","text":"{ping.0.name}","font":"sans","size":16},
        {"type":"text","text":"{ping.0.status}","color":"{ping.0.color}"}
      ]},
      {"type":"box","w":34,"style":{"gap":1},"children":[
        {"type":"text","text":"{ping.0.ms}","font":"bold","size":18,"align":"right"},
        {"type":"text","text":"ms","align":"right","color":"#7c8794"}
      ]}
    ]},
    {"type":"box","style":{"direction":"row","gap":4,"align":"center","padding":7,"radius":10,"background":"#141a22","gradient":"#0b0e13","border":"{ping.1.color}"},"children":[
      {"type":"box","w":27,"h":27,"children":[
        {"type":"circle","x":0,"y":0,"r":13,"color":"#10161d","style":{"border":"{ping.1.color}","borderWidth":2}},
        {"type":"circle","x":6,"y":6,"r":7,"color":"{ping.1.color}"}
      ]},
      {"type":"box","w":70,"style":{"gap":3},"children":[
        {"type":"text","text":"{ping.1.name}","font":"sans","size":16},
        {"type":"text","text":"{ping.1.status}","color":"{ping.1.color}"}
      ]},
      {"type":"box","w":34,"style":{"gap":1},"children":[
        {"type":"text","text":"{ping.1.ms}","font":"bold","size":18,"align":"right"},
        {"type":"text","text":"ms","align":"right","color":"#7c8794"}
      ]}
    ]},
    {"type":"box","style":{"direction":"row","gap":4,"align":"center","padding":7,"radius":10,"background":"#141a22","gradient":"#0b0e13","border":"{ping.2.color}"},"children":[
      {"type":"box","w":27,"h":27,"children":[
        {"type":"circle","x":0,"y":0,"r":13,"color":"#10161d","style":{"border":"{ping.2.color}","borderWidth":2}},
        {"type":"circle","x":6,"y":6,"r":7,"color":"{ping.2.color}"}
      ]},
      {"type":"box","w":70,"style":{"gap":3},"children":[
        {"type":"text","text":"{ping.2.name}","font":"sans","size":16},
        {"type":"text","text":"{ping.2.status}","color":"{ping.2.color}"}
      ]},
      {"type":"box","w":34,"style":{"gap":1},"children":[
        {"type":"text","text":"{ping.2.ms}","font":"bold","size":18,"align":"right"},
        {"type":"text","text":"ms","align":"right","color":"#7c8794"}
      ]}
    ]},
    {"type":"box","style":{"gap":8,"radius":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","padding":8},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"wifi","color":"#7c8794"},
        {"type":"text","text":"{wifi.rssi} dBm","align":"right","color":"{wifi.color}"}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"uptime","color":"#7c8794"},
        {"type":"text","text":"{uptime}","align":"right"}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"address","color":"#7c8794"},
        {"type":"text","text":"{wifi.ip}","align":"right"}
      ]}
    ]},
    {"type":"text","x":77,"y":296,"text":"{wifi.ssid}","align":"center","color":"dim"}
  ]
})json";

static const char LATENCY_METERS[] = R"json({
  "name": "Latency Meters",
  "led": {"color":"{ping.0.color}","mode":"breathe","speed":3000,"rules":[{"when":"ping.0.ms > 100 || ping.1.ms > 100 || ping.2.ms > 100","color":"warn","mode":"solid"}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"LATENCY","color":"#7c8794"},
      {"type":"text","text":"0-100 ms","align":"right","color":"#7c8794"}
    ]},
    {"type":"box","style":{"gap":6,"padding":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","style":{"direction":"row","justify":"between","align":"center"},"children":[
        {"type":"text","text":"{ping.0.name}","font":"sans","size":16},
        {"type":"box","style":{"direction":"row","gap":3,"align":"end"},"children":[
          {"type":"text","text":"{ping.0.ms}","font":"bold","size":20,"color":"{hsv(clamp(130-ping.0.ms*1.3,0,130),85,95)}"},
          {"type":"text","text":"ms","color":"#7c8794"}
        ]}
      ]},
      {"type":"bar","h":10,"value":"{min(ping.0.ms,100)}","color":"#14804a","style":{"gradient":"{hsv(clamp(130-ping.0.ms*1.3,0,130),85,95)}","gradientDir":"right","radius":5,"background":"#1a2029"}}
    ]},
    {"type":"box","style":{"gap":6,"padding":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","style":{"direction":"row","justify":"between","align":"center"},"children":[
        {"type":"text","text":"{ping.1.name}","font":"sans","size":16},
        {"type":"box","style":{"direction":"row","gap":3,"align":"end"},"children":[
          {"type":"text","text":"{ping.1.ms}","font":"bold","size":20,"color":"{hsv(clamp(130-ping.1.ms*1.3,0,130),85,95)}"},
          {"type":"text","text":"ms","color":"#7c8794"}
        ]}
      ]},
      {"type":"bar","h":10,"value":"{min(ping.1.ms,100)}","color":"#14804a","style":{"gradient":"{hsv(clamp(130-ping.1.ms*1.3,0,130),85,95)}","gradientDir":"right","radius":5,"background":"#1a2029"}}
    ]},
    {"type":"box","style":{"gap":6,"padding":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","style":{"direction":"row","justify":"between","align":"center"},"children":[
        {"type":"text","text":"{ping.2.name}","font":"sans","size":16},
        {"type":"box","style":{"direction":"row","gap":3,"align":"end"},"children":[
          {"type":"text","text":"{ping.2.ms}","font":"bold","size":20,"color":"{hsv(clamp(130-ping.2.ms*1.3,0,130),85,95)}"},
          {"type":"text","text":"ms","color":"#7c8794"}
        ]}
      ]},
      {"type":"bar","h":10,"value":"{min(ping.2.ms,100)}","color":"#14804a","style":{"gradient":"{hsv(clamp(130-ping.2.ms*1.3,0,130),85,95)}","gradientDir":"right","radius":5,"background":"#1a2029"}}
    ]},
    {"type":"box","style":{"gap":6,"radius":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","padding":8},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"WIFI","color":"#7c8794"},
        {"type":"text","text":"{wifi.rssi} dBm","align":"right","color":"#c4ccd6"}
      ]},
      {"type":"bar","h":8,"value":"{wifi.pct}","color":"#c0392b","style":{"gradient":"#2fd37a","gradientDir":"right","radius":4,"background":"#1a2029"}}
    ]},
    {"type":"text","x":77,"y":296,"text":"{wifi.ssid}  ·  {time}","align":"center","color":"dim"}
  ]
})json";

static const char SERVER_RACK[] = R"json({
  "name": "Server Rack",
  "led": {"color":"ok","mode":"solid","brightness":40,"rules":[{"key":"ping.0.status","is":"down","color":"bad","mode":"blink","speed":300},{"key":"ping.1.status","is":"down","color":"bad","mode":"blink","speed":300},{"key":"ping.2.status","is":"down","color":"bad","mode":"blink","speed":300}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"SERVER RACK","color":"#7c8794"},
      {"type":"text","text":"{ping.count} targets, ms","align":"right","color":"#c4ccd6"}
    ]},
    {"type":"box","style":{"padding":3,"gap":4,"radius":7,"background":"#05070a","border":"#3a4656","borderWidth":2},"children":[
      {"type":"box","h":30,"style":{"direction":"row","gap":3,"align":"center","padding":5,"radius":5,"background":"#161c25","gradient":"#0c1016","border":"#2c3644"},"children":[
        {"type":"rect","w":3,"h":14,"color":"{ping.0.color}","fill":true,"style":{"radius":1}},
        {"type":"text","w":60,"text":"{ping.0.name}","color":"#c4ccd6"},
        {"type":"text","w":48,"text":"{ping.0.bars}","color":"{ping.0.color}"},
        {"type":"text","w":18,"text":"{ping.0.ms}","align":"right"}
      ]},
      {"type":"box","h":30,"style":{"direction":"row","gap":3,"align":"center","padding":5,"radius":5,"background":"#161c25","gradient":"#0c1016","border":"#2c3644"},"children":[
        {"type":"rect","w":3,"h":14,"color":"{ping.1.color}","fill":true,"style":{"radius":1}},
        {"type":"text","w":60,"text":"{ping.1.name}","color":"#c4ccd6"},
        {"type":"text","w":48,"text":"{ping.1.bars}","color":"{ping.1.color}"},
        {"type":"text","w":18,"text":"{ping.1.ms}","align":"right"}
      ]},
      {"type":"box","h":30,"style":{"direction":"row","gap":3,"align":"center","padding":5,"radius":5,"background":"#161c25","gradient":"#0c1016","border":"#2c3644"},"children":[
        {"type":"rect","w":3,"h":14,"color":"{ping.2.color}","fill":true,"style":{"radius":1}},
        {"type":"text","w":60,"text":"{ping.2.name}","color":"#c4ccd6"},
        {"type":"text","w":48,"text":"{ping.2.bars}","color":"{ping.2.color}"},
        {"type":"text","w":18,"text":"{ping.2.ms}","align":"right"}
      ]},
      {"type":"box","h":"{if(ping.count>3,30,0)}","style":{"direction":"row","gap":3,"align":"center","padding":5,"radius":5,"background":"#161c25","gradient":"#0c1016","border":"#2c3644"},"children":[
        {"type":"rect","w":3,"h":14,"color":"{ping.3.color}","fill":true,"style":{"radius":1}},
        {"type":"text","w":60,"text":"{ping.3.name}","color":"#c4ccd6"},
        {"type":"text","w":48,"text":"{ping.3.bars}","color":"{ping.3.color}"},
        {"type":"text","w":18,"text":"{ping.3.ms}","align":"right"}
      ]},
      {"type":"box","h":"{if(ping.count>4,30,0)}","style":{"direction":"row","gap":3,"align":"center","padding":5,"radius":5,"background":"#161c25","gradient":"#0c1016","border":"#2c3644"},"children":[
        {"type":"rect","w":3,"h":14,"color":"{ping.4.color}","fill":true,"style":{"radius":1}},
        {"type":"text","w":60,"text":"{ping.4.name}","color":"#c4ccd6"},
        {"type":"text","w":48,"text":"{ping.4.bars}","color":"{ping.4.color}"},
        {"type":"text","w":18,"text":"{ping.4.ms}","align":"right"}
      ]},
      {"type":"box","h":"{if(ping.count>5,30,0)}","style":{"direction":"row","gap":3,"align":"center","padding":5,"radius":5,"background":"#161c25","gradient":"#0c1016","border":"#2c3644"},"children":[
        {"type":"rect","w":3,"h":14,"color":"{ping.5.color}","fill":true,"style":{"radius":1}},
        {"type":"text","w":60,"text":"{ping.5.name}","color":"#c4ccd6"},
        {"type":"text","w":48,"text":"{ping.5.bars}","color":"{ping.5.color}"},
        {"type":"text","w":18,"text":"{ping.5.ms}","align":"right"}
      ]}
    ]},
    {"type":"box","style":{"gap":8,"radius":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","padding":8},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"network","color":"#7c8794"},
        {"type":"text","text":"{wifi.ssid}","align":"right"}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"IP","color":"#7c8794"},
        {"type":"text","text":"{wifi.ip}","align":"right"}
      ]}
    ]},
    {"type":"text","x":77,"y":296,"text":"{time}  {date.day} {date.md}","align":"center","color":"dim"}
  ]
})json";

static const char DASHBOARD[] = R"json({
  "name": "Dashboard",
  "led": {"color":"{ping.0.color}","mode":"solid","rules":[{"when":"wifi.rssi < -80","color":"warn","mode":"blink","speed":1000},{"key":"ping.0.status","is":"down","mode":"blink","speed":400}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between","align":"center","padding":8,"radius":12,"background":"{hsv(215,70,8+26*clamp(1-abs(time.hour-13)/7,0,1))}","gradient":"#05070a","border":"#1c2535"},"children":[
      {"type":"text","text":"{time}","font":"bold","size":36},
      {"type":"box","style":{"gap":4},"children":[
        {"type":"text","text":"{date.day}","align":"right","color":"#c4ccd6"},
        {"type":"text","text":"{date.md}","align":"right","color":"#c4ccd6"}
      ]}
    ]},
    {"type":"box","style":{"gap":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10,"padding":8},"children":[
      {"type":"text","text":"TARGETS","color":"#7c8794"},
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","r":4,"color":"{ping.0.color}"},
        {"type":"text","w":70,"text":"{ping.0.name}","color":"#c4ccd6"},
        {"type":"text","w":52,"text":"{ping.0.ms} ms {ping.0.trend}","align":"right","color":"{ping.0.color}"}
      ]},
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","r":4,"color":"{ping.1.color}"},
        {"type":"text","w":70,"text":"{ping.1.name}","color":"#c4ccd6"},
        {"type":"text","w":52,"text":"{ping.1.ms} ms {ping.1.trend}","align":"right","color":"{ping.1.color}"}
      ]},
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","r":4,"color":"{ping.2.color}"},
        {"type":"text","w":70,"text":"{ping.2.name}","color":"#c4ccd6"},
        {"type":"text","w":52,"text":"{ping.2.ms} ms {ping.2.trend}","align":"right","color":"{ping.2.color}"}
      ]}
    ]},
    {"type":"box","style":{"gap":6,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10,"padding":8},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"WIFI","color":"#7c8794"},
        {"type":"text","text":"{wifi.ssid}","align":"right","color":"#c4ccd6"}
      ]},
      {"type":"bar","h":8,"value":"{wifi.pct}","color":"#c0392b","style":{"gradient":"#2fd37a","gradientDir":"right","radius":4,"background":"#1a2029"}},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"{wifi.bars}","color":"{wifi.color}"},
        {"type":"text","text":"{wifi.rssi} dBm","align":"right"}
      ]}
    ]},
    {"type":"box","style":{"gap":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10,"padding":8},"children":[
      {"type":"text","text":"SYSTEM","color":"#7c8794"},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"{hostname}","color":"#7c8794"},
        {"type":"text","text":"{wifi.ip}","align":"right"}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"up {uptime}","color":"#7c8794"},
        {"type":"text","text":"heap {heap}","align":"right"}
      ]}
    ]}
  ]
})json";

static const char SIGNAL_METER[] = R"json({
  "name": "Signal Meter",
  "led": {"color":"{wifi.color}","mode":"solid","rules":[{"when":"wifi.rssi < -85","mode":"blink","speed":500}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"WIFI SIGNAL","color":"#7c8794"},
      {"type":"text","text":"{wifi.pct}%","align":"right","color":"{wifi.color}"}
    ]},
    {"type":"box","w":154,"h":126,"children":[
      {"type":"arc","x":7,"y":0,"w":140,"value":"{wifi.pct}","start":225,"end":495,"color":"{wifi.color}","style":{"thickness":11,"background":"#1a2029"}},
      {"type":"text","x":77,"y":36,"text":"{wifi.rssi}","font":"bold","size":46,"align":"center"},
      {"type":"text","x":77,"y":86,"text":"dBm","align":"center","color":"#c4ccd6"}
    ]},
    {"type":"box","style":{"direction":"row","gap":8,"align":"center","padding":8,"radius":12,"background":"#10304c","gradient":"#070b10","border":"#1c3a5c"},"children":[
      {"type":"box","w":45,"h":42,"children":[
        {"type":"rect","x":0,"y":30,"w":9,"h":12,"color":"{if(wifi.rssi>-85,rgb(61,220,132),rgb(36,44,56))}","fill":true,"style":{"radius":3}},
        {"type":"rect","x":12,"y":20,"w":9,"h":22,"color":"{if(wifi.rssi>-75,rgb(61,220,132),rgb(36,44,56))}","fill":true,"style":{"radius":3}},
        {"type":"rect","x":24,"y":10,"w":9,"h":32,"color":"{if(wifi.rssi>-65,rgb(61,220,132),rgb(36,44,56))}","fill":true,"style":{"radius":3}},
        {"type":"rect","x":36,"y":0,"w":9,"h":42,"color":"{if(wifi.rssi>-55,rgb(61,220,132),rgb(36,44,56))}","fill":true,"style":{"radius":3}}
      ]},
      {"type":"box","style":{"gap":6},"children":[
        {"type":"text","text":"{wifi.ssid}","font":"bold","size":16},
        {"type":"text","text":"{wifi.ip}","color":"#c4ccd6"}
      ]}
    ]},
    {"type":"box","h":26,"children":[
      {"type":"rect","x":0,"y":4,"w":154,"h":6,"color":"#c0392b","fill":true,"style":{"radius":3,"gradient":"#2fd37a","gradientDir":"right"}},
      {"type":"circle","x":"{clamp(wifi.pct*1.44,0,144)}","y":2,"r":5,"color":"text","style":{"border":"bg","borderWidth":2}},
      {"type":"text","x":0,"y":16,"text":"-90","color":"#7c8794"},
      {"type":"text","x":77,"y":16,"text":"dBm","align":"center","color":"#7c8794"},
      {"type":"text","x":154,"y":16,"text":"-40","align":"right","color":"#7c8794"}
    ]},
    {"type":"text","x":77,"y":296,"text":"{hostname}.local","align":"center","color":"dim"}
  ]
})json";

static const char NETWORK[] = R"json({
  "name": "Network",
  "led": {"color":"{wifi.color}","mode":"breathe","speed":3000,"rules":[{"when":"wifi.rssi < -85","color":"bad","mode":"blink","speed":500}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"NETWORK","color":"#7c8794"},
      {"type":"text","text":"{wifi.bars}","align":"right","color":"{wifi.color}"}
    ]},
    {"type":"box","style":{"gap":3,"padding":7,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"text","text":"HOSTNAME","color":"#7c8794"},
      {"type":"text","text":"{hostname}","font":"bold","size":20}
    ]},
    {"type":"box","style":{"gap":3,"padding":7,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"text","text":"IP ADDRESS","color":"#7c8794"},
      {"type":"text","text":"{wifi.ip}","font":"bold","size":17}
    ]},
    {"type":"box","style":{"gap":5,"padding":7,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"WIFI","color":"#7c8794"},
        {"type":"text","text":"{wifi.rssi} dBm","align":"right","color":"{wifi.color}"}
      ]},
      {"type":"text","text":"{wifi.ssid}","font":"bold","size":17},
      {"type":"bar","h":8,"value":"{wifi.pct}","color":"#c0392b","style":{"gradient":"#2fd37a","gradientDir":"right","radius":4,"background":"#1a2029"}}
    ]},
    {"type":"box","style":{"gap":8,"padding":7,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"uptime","color":"#7c8794"},
        {"type":"text","text":"{uptime}","align":"right"}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"free heap","color":"#7c8794"},
        {"type":"text","text":"{heap}","align":"right"}
      ]},
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"time","color":"#7c8794"},
        {"type":"text","text":"{time.sec}","align":"right"}
      ]}
    ]},
    {"type":"text","x":77,"y":296,"text":"{date}","align":"center","color":"dim"}
  ]
})json";

// Needs a data source called "weather" with fields temp, humidity and
// wind, e.g. Open-Meteo (see README, "Data sources"). Not preloaded
// because it shows "--" until that source exists.
static const char WEATHER[] = R"json({
  "name": "Weather",
  "led": {"color":"{api.weather.color}","mode":"solid","brightness":40,"rules":[{"key":"api.weather.status","is":"error","mode":"blink","speed":800},{"when":"api.weather.temp >= 30","color":"#ff4000","mode":"breathe","speed":2500},{"when":"api.weather.temp <= 0","color":"#40a0ff","mode":"breathe","speed":2500}]},
  "style": {"direction":"column","gap":6,"padding":8},
  "elements": [
    {"type":"box","style":{"direction":"row","justify":"between"},"children":[
      {"type":"text","text":"WEATHER","color":"#7c8794"},
      {"type":"text","text":"{api.weather.age}","align":"right","color":"{api.weather.color}"}
    ]},
    {"type":"box","style":{"padding":12,"radius":14,"background":"{hsv(clamp(220-(api.weather.temp+10)*5,0,220),70,40)}","gradient":"{hsv(clamp(220-(api.weather.temp+10)*5,0,220),70,12)}"},"children":[
      {"type":"text","text":"{api.weather.temp}°","font":"bold","size":76,"align":"center"},
      {"type":"text","text":"degrees C","font":"sans","size":16,"align":"center","color":"#d0d8e0"}
    ]},
    {"type":"box","h":26,"children":[
      {"type":"rect","x":0,"y":4,"w":154,"h":6,"color":"#2a6df0","fill":true,"style":{"radius":3,"gradient":"#ff5a2a","gradientDir":"right"}},
      {"type":"circle","x":"{clamp((api.weather.temp+10)*3.2,0,144)}","y":2,"r":5,"color":"text","style":{"border":"bg","borderWidth":2}},
      {"type":"text","x":0,"y":16,"text":"-10°","color":"#7c8794"},
      {"type":"text","x":154,"y":16,"text":"35°","align":"right","color":"#7c8794"}
    ]},
    {"type":"box","style":{"direction":"row","gap":6},"children":[
      {"type":"box","w":74,"style":{"gap":4,"padding":7,"radius":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38"},"children":[
        {"type":"text","text":"HUMIDITY","color":"#7c8794"},
        {"type":"text","text":"{api.weather.humidity}%","font":"bold","size":22},
        {"type":"bar","h":6,"value":"{api.weather.humidity}","color":"#1d5fd0","style":{"gradient":"#40d0ff","gradientDir":"right","radius":3,"background":"#1a2029"}}
      ]},
      {"type":"box","w":74,"style":{"gap":4,"padding":7,"radius":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38"},"children":[
        {"type":"text","text":"WIND km/h","color":"#7c8794"},
        {"type":"text","text":"{api.weather.wind}","font":"bold","size":22},
        {"type":"bar","h":6,"value":"{min(api.weather.wind*2,100)}","color":"#1c8a4a","style":{"gradient":"#ffd24a","gradientDir":"right","radius":3,"background":"#1a2029"}}
      ]}
    ]},
    {"type":"box","style":{"radius":8,"background":"#141a22","gradient":"#0b0e13","border":"#252d38","padding":8},"children":[
      {"type":"box","style":{"direction":"row","justify":"between"},"children":[
        {"type":"text","text":"updated","color":"#7c8794"},
        {"type":"text","text":"{api.weather.updated}","align":"right"}
      ]}
    ]},
    {"type":"text","x":77,"y":296,"text":"{time}  {date.day} {date.md}","align":"center","color":"dim"}
  ]
})json";

// Flow layout: no x/y, elements stack top to bottom inside boxes, with
// named styles, a ring gauge, status dots and a rounded card.
static const char CARDS[] = R"json({
  "name": "Cards",
  "led": {"color":"{ping.0.color}","mode":"breathe","speed":3000,"rules":[{"key":"ping.0.status","is":"down","color":"bad","mode":"blink","speed":400},{"key":"ping.1.status","is":"down","color":"bad","mode":"blink","speed":400},{"key":"ping.2.status","is":"down","color":"bad","mode":"blink","speed":400}]},
  "style": {"direction":"column","gap":8,"padding":6},
  "styles": {
    "label": {"color":"#7c8794"},
    "card": {"background":"#141a22","gradient":"#0b0e13","border":"#252d38","radius":10,"padding":8,"gap":5},
    "big": {"font":"bold","size":44,"align":"center"}
  },
  "elements": [
    {"type":"box","class":"card","children":[
      {"type":"text","class":"label","text":"TIME"},
      {"type":"text","class":"big","text":"{time}"},
      {"type":"text","text":"{date.day} {date.md}","font":"sans","size":14,"align":"center","color":"#c4ccd6"}
    ]},
    {"type":"box","class":"card","style":{"direction":"row","align":"center","gap":10},"children":[
      {"type":"arc","w":56,"value":"{wifi.pct}","color":"{wifi.color}","style":{"thickness":7,"background":"#1a2029"}},
      {"type":"box","style":{"gap":3},"children":[
        {"type":"text","class":"label","text":"WIFI"},
        {"type":"text","text":"{wifi.rssi} dBm","font":"bold","size":18},
        {"type":"text","text":"{wifi.ssid}","color":"#c4ccd6"}
      ]}
    ]},
    {"type":"box","class":"card","children":[
      {"type":"text","class":"label","text":"SERVERS 🖥"},
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","w":10,"color":"{ping.0.color}"},
        {"type":"text","w":80,"text":"{ping.0.name}"},
        {"type":"text","w":42,"text":"{ping.0.ms} ms","align":"right"}
      ]},
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","w":10,"color":"{ping.1.color}"},
        {"type":"text","w":80,"text":"{ping.1.name}"},
        {"type":"text","w":42,"text":"{ping.1.ms} ms","align":"right"}
      ]},
      {"type":"box","style":{"direction":"row","gap":4,"align":"center"},"children":[
        {"type":"circle","w":10,"color":"{ping.2.color}"},
        {"type":"text","w":80,"text":"{ping.2.name}"},
        {"type":"text","w":42,"text":"{ping.2.ms} ms","align":"right"}
      ]}
    ]},
    {"type":"box","style":{"direction":"row","justify":"between","align":"center"},"children":[
      {"type":"triangle","points":[[0,8],[5,0],[10,8]],"color":"accent"},
      {"type":"text","text":"up {uptime}","color":"#7c8794"},
      {"type":"polygon","points":[[4,0],[8,3],[6,8],[2,8],[0,3]],"color":"ok"}
    ]}
  ]
})json";

const LayoutTemplate LAYOUT_TEMPLATES[] = {
  { "blank",          "Blank",          false, BLANK },
  { "big-clock",      "Big Clock",      true,  BIG_CLOCK },
  { "stacked-clock",  "Stacked Clock",  true,  STACKED_CLOCK },
  { "night-clock",    "Night Clock",    false, NIGHT_CLOCK },
  { "date-card",      "Date Card",      false, DATE_CARD },
  { "ping-board",     "Ping Board",     true,  PING_BOARD },
  { "status-lights",  "Status Lights",  true,  STATUS_LIGHTS },
  { "latency-hero",   "Latency Hero",   true,  LATENCY_HERO },
  { "latency-meters", "Latency Meters", true,  LATENCY_METERS },
  { "server-rack",    "Server Rack",    false, SERVER_RACK },
  { "dashboard",      "Dashboard",      true,  DASHBOARD },
  { "signal-meter",   "Signal Meter",   false, SIGNAL_METER },
  { "network",        "Network",        true,  NETWORK },
  { "weather",        "Weather",        false, WEATHER },
  { "cards",          "Cards",          false, CARDS },
};

const uint8_t LAYOUT_TEMPLATE_COUNT = sizeof(LAYOUT_TEMPLATES) / sizeof(LAYOUT_TEMPLATES[0]);
