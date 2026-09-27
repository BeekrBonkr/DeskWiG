#include "LayoutTemplates.h"

// Screen is 170 x 320. Font is 6 x 8 per char times size: size 1 fits 28
// columns, size 2 fits 14, size 4 fits 7, size 8 fits 3. Default config has
// three ping targets, so preloaded layouts show ping.0 to ping.2.

static const char BLANK[] = R"json({
  "name": "My Widget",
  "elements": [
    {"type":"text","x":85,"y":150,"size":2,"align":"center","color":"text","text":"Hello"}
  ]
})json";

static const char BIG_CLOCK[] = R"json({
  "name": "Big Clock",
  "elements": [
    {"type":"text","x":85,"y":96,"size":4,"align":"center","color":"text","text":"{time}"},
    {"type":"text","x":85,"y":134,"size":2,"align":"center","color":"dim","text":"{time.ampm}"},
    {"type":"text","x":85,"y":176,"size":2,"align":"center","color":"text","text":"{date.day} {date.md}"},
    {"type":"line","x":6,"y":296,"x2":164,"y2":296,"color":"dim"},
    {"type":"text","x":6,"y":304,"size":1,"color":"dim","text":"{wifi.ssid}"},
    {"type":"text","x":164,"y":304,"size":1,"align":"right","color":"{wifi.color}","text":"{wifi.bars}"}
  ]
})json";

static const char STACKED_CLOCK[] = R"json({
  "name": "Stacked Clock",
  "elements": [
    {"type":"text","x":85,"y":52,"size":8,"align":"center","color":"text","text":"{time.hour}"},
    {"type":"text","x":85,"y":132,"size":8,"align":"center","color":"accent","text":"{time.min}"},
    {"type":"text","x":85,"y":216,"size":2,"align":"center","color":"dim","text":"{time.ampm}"},
    {"type":"text","x":85,"y":248,"size":2,"align":"center","color":"text","text":"{date.day} {date.md}"},
    {"type":"text","x":85,"y":304,"size":1,"align":"center","color":"dim","text":"{wifi.ssid}"}
  ]
})json";

static const char NIGHT_CLOCK[] = R"json({
  "name": "Night Clock",
  "elements": [
    {"type":"text","x":85,"y":130,"size":4,"align":"center","color":"dim","text":"{time}"},
    {"type":"text","x":85,"y":170,"size":1,"align":"center","color":"dim","text":"{date.day} {date.md}"}
  ]
})json";

static const char DATE_CARD[] = R"json({
  "name": "Date Card",
  "elements": [
    {"type":"text","x":85,"y":70,"size":2,"align":"center","color":"dim","text":"{date.dow}"},
    {"type":"text","x":85,"y":110,"size":4,"align":"center","color":"text","text":"{date.md}"},
    {"type":"text","x":85,"y":152,"size":2,"align":"center","color":"dim","text":"{date.year}"},
    {"type":"line","x":40,"y":196,"x2":130,"y2":196,"color":"dim"},
    {"type":"text","x":85,"y":212,"size":2,"align":"center","color":"accent","text":"{time}"},
    {"type":"text","x":85,"y":304,"size":1,"align":"center","color":"dim","text":"{hostname}"}
  ]
})json";

static const char PING_BOARD[] = R"json({
  "name": "Ping Board",
  "elements": [
    {"type":"text","x":6,"y":12,"size":1,"color":"text","text":"{wifi.ssid}"},
    {"type":"text","x":164,"y":12,"size":1,"align":"right","color":"{wifi.color}","text":"{wifi.bars}"},
    {"type":"line","x":6,"y":26,"x2":164,"y2":26,"color":"dim"},
    {"type":"text","x":6,"y":36,"size":1,"color":"dim","text":"TARGETS (ms)"},
    {"type":"text","x":6,"y":59,"size":1,"color":"text","text":"{ping.0.name}"},
    {"type":"text","x":164,"y":54,"size":2,"align":"right","color":"{ping.0.color}","text":"{ping.0.ms}"},
    {"type":"text","x":6,"y":89,"size":1,"color":"text","text":"{ping.1.name}"},
    {"type":"text","x":164,"y":84,"size":2,"align":"right","color":"{ping.1.color}","text":"{ping.1.ms}"},
    {"type":"text","x":6,"y":119,"size":1,"color":"text","text":"{ping.2.name}"},
    {"type":"text","x":164,"y":114,"size":2,"align":"right","color":"{ping.2.color}","text":"{ping.2.ms}"},
    {"type":"line","x":6,"y":148,"x2":164,"y2":148,"color":"dim"},
    {"type":"text","x":6,"y":158,"size":1,"color":"dim","text":"HISTORY"},
    {"type":"text","x":6,"y":172,"size":1,"color":"{ping.0.color}","text":"{ping.0.bars} {ping.0.name}"},
    {"type":"text","x":6,"y":186,"size":1,"color":"{ping.1.color}","text":"{ping.1.bars} {ping.1.name}"},
    {"type":"text","x":6,"y":200,"size":1,"color":"{ping.2.color}","text":"{ping.2.bars} {ping.2.name}"},
    {"type":"line","x":6,"y":296,"x2":164,"y2":296,"color":"dim"},
    {"type":"text","x":6,"y":304,"size":1,"color":"dim","text":"IP {wifi.ip}"},
    {"type":"text","x":164,"y":304,"size":1,"align":"right","color":"dim","text":"{time}"}
  ]
})json";

static const char LATENCY_HERO[] = R"json({
  "name": "Latency",
  "elements": [
    {"type":"text","x":85,"y":40,"size":2,"align":"center","color":"dim","text":"{ping.0.name}"},
    {"type":"text","x":85,"y":80,"size":4,"align":"center","color":"{ping.0.color}","text":"{ping.0.ms}"},
    {"type":"text","x":85,"y":116,"size":1,"align":"center","color":"dim","text":"milliseconds {ping.0.trend}"},
    {"type":"text","x":85,"y":140,"size":2,"align":"center","color":"{ping.0.color}","text":"{ping.0.bars}"},
    {"type":"line","x":6,"y":176,"x2":164,"y2":176,"color":"dim"},
    {"type":"text","x":6,"y":190,"size":1,"color":"dim","text":"WIFI {wifi.rssi} dBm"},
    {"type":"bar","x":6,"y":204,"w":158,"h":8,"color":"{wifi.color}","value":"{wifi.pct}"},
    {"type":"text","x":6,"y":304,"size":1,"color":"dim","text":"up {uptime}"},
    {"type":"text","x":164,"y":304,"size":1,"align":"right","color":"dim","text":"{time}"}
  ]
})json";

static const char STATUS_LIGHTS[] = R"json({
  "name": "Status Lights",
  "elements": [
    {"type":"text","x":6,"y":12,"size":1,"color":"dim","text":"STATUS"},
    {"type":"text","x":164,"y":12,"size":1,"align":"right","color":"dim","text":"{time}"},
    {"type":"line","x":6,"y":26,"x2":164,"y2":26,"color":"dim"},
    {"type":"rect","x":6,"y":44,"w":14,"h":14,"fill":true,"color":"{ping.0.color}"},
    {"type":"text","x":28,"y":47,"size":1,"color":"text","text":"{ping.0.name}"},
    {"type":"text","x":164,"y":47,"size":1,"align":"right","color":"{ping.0.color}","text":"{ping.0.ms} ms"},
    {"type":"rect","x":6,"y":76,"w":14,"h":14,"fill":true,"color":"{ping.1.color}"},
    {"type":"text","x":28,"y":79,"size":1,"color":"text","text":"{ping.1.name}"},
    {"type":"text","x":164,"y":79,"size":1,"align":"right","color":"{ping.1.color}","text":"{ping.1.ms} ms"},
    {"type":"rect","x":6,"y":108,"w":14,"h":14,"fill":true,"color":"{ping.2.color}"},
    {"type":"text","x":28,"y":111,"size":1,"color":"text","text":"{ping.2.name}"},
    {"type":"text","x":164,"y":111,"size":1,"align":"right","color":"{ping.2.color}","text":"{ping.2.ms} ms"},
    {"type":"line","x":6,"y":140,"x2":164,"y2":140,"color":"dim"},
    {"type":"text","x":6,"y":154,"size":1,"color":"dim","text":"WIFI"},
    {"type":"text","x":164,"y":154,"size":1,"align":"right","color":"{wifi.color}","text":"{wifi.bars} {wifi.rssi} dBm"},
    {"type":"text","x":6,"y":168,"size":1,"color":"dim","text":"UPTIME"},
    {"type":"text","x":164,"y":168,"size":1,"align":"right","color":"text","text":"{uptime}"},
    {"type":"text","x":6,"y":304,"size":1,"color":"dim","text":"{wifi.ssid}"},
    {"type":"text","x":164,"y":304,"size":1,"align":"right","color":"dim","text":"{wifi.ip}"}
  ]
})json";

static const char LATENCY_METERS[] = R"json({
  "name": "Latency Meters",
  "elements": [
    {"type":"text","x":6,"y":12,"size":1,"color":"dim","text":"LATENCY  0-100 ms"},
    {"type":"line","x":6,"y":26,"x2":164,"y2":26,"color":"dim"},
    {"type":"text","x":6,"y":40,"size":1,"color":"text","text":"{ping.0.name}"},
    {"type":"text","x":164,"y":40,"size":1,"align":"right","color":"{ping.0.color}","text":"{ping.0.ms} ms"},
    {"type":"bar","x":6,"y":54,"w":158,"h":10,"color":"{ping.0.color}","value":"{ping.0.ms}"},
    {"type":"text","x":6,"y":82,"size":1,"color":"text","text":"{ping.1.name}"},
    {"type":"text","x":164,"y":82,"size":1,"align":"right","color":"{ping.1.color}","text":"{ping.1.ms} ms"},
    {"type":"bar","x":6,"y":96,"w":158,"h":10,"color":"{ping.1.color}","value":"{ping.1.ms}"},
    {"type":"text","x":6,"y":124,"size":1,"color":"text","text":"{ping.2.name}"},
    {"type":"text","x":164,"y":124,"size":1,"align":"right","color":"{ping.2.color}","text":"{ping.2.ms} ms"},
    {"type":"bar","x":6,"y":138,"w":158,"h":10,"color":"{ping.2.color}","value":"{ping.2.ms}"},
    {"type":"line","x":6,"y":170,"x2":164,"y2":170,"color":"dim"},
    {"type":"text","x":6,"y":184,"size":1,"color":"dim","text":"WIFI {wifi.rssi} dBm"},
    {"type":"bar","x":6,"y":198,"w":158,"h":10,"color":"{wifi.color}","value":"{wifi.pct}"},
    {"type":"text","x":6,"y":304,"size":1,"color":"dim","text":"{wifi.ssid}"},
    {"type":"text","x":164,"y":304,"size":1,"align":"right","color":"dim","text":"{time}"}
  ]
})json";

static const char SERVER_RACK[] = R"json({
  "name": "Server Rack",
  "elements": [
    {"type":"text","x":6,"y":12,"size":1,"color":"text","text":"{wifi.ssid}"},
    {"type":"text","x":164,"y":12,"size":1,"align":"right","color":"{wifi.color}","text":"{wifi.bars}"},
    {"type":"line","x":6,"y":26,"x2":164,"y2":26,"color":"dim"},
    {"type":"text","x":6,"y":40,"size":1,"color":"{ping.0.color}","text":"{ping.0.bars} {ping.0.name}"},
    {"type":"text","x":164,"y":40,"size":1,"align":"right","color":"{ping.0.color}","text":"{ping.0.ms}"},
    {"type":"text","x":6,"y":58,"size":1,"color":"{ping.1.color}","text":"{ping.1.bars} {ping.1.name}"},
    {"type":"text","x":164,"y":58,"size":1,"align":"right","color":"{ping.1.color}","text":"{ping.1.ms}"},
    {"type":"text","x":6,"y":76,"size":1,"color":"{ping.2.color}","text":"{ping.2.bars} {ping.2.name}"},
    {"type":"text","x":164,"y":76,"size":1,"align":"right","color":"{ping.2.color}","text":"{ping.2.ms}"},
    {"type":"text","x":6,"y":94,"size":1,"color":"{ping.3.color}","text":"{ping.3.bars} {ping.3.name}"},
    {"type":"text","x":164,"y":94,"size":1,"align":"right","color":"{ping.3.color}","text":"{ping.3.ms}"},
    {"type":"text","x":6,"y":112,"size":1,"color":"{ping.4.color}","text":"{ping.4.bars} {ping.4.name}"},
    {"type":"text","x":164,"y":112,"size":1,"align":"right","color":"{ping.4.color}","text":"{ping.4.ms}"},
    {"type":"text","x":6,"y":130,"size":1,"color":"{ping.5.color}","text":"{ping.5.bars} {ping.5.name}"},
    {"type":"text","x":164,"y":130,"size":1,"align":"right","color":"{ping.5.color}","text":"{ping.5.ms}"},
    {"type":"line","x":6,"y":148,"x2":164,"y2":148,"color":"dim"},
    {"type":"text","x":6,"y":158,"size":1,"color":"dim","text":"{ping.count} targets, ms"},
    {"type":"text","x":6,"y":304,"size":1,"color":"dim","text":"IP {wifi.ip}"},
    {"type":"text","x":164,"y":304,"size":1,"align":"right","color":"dim","text":"{time}"}
  ]
})json";

static const char DASHBOARD[] = R"json({
  "name": "Dashboard",
  "elements": [
    {"type":"text","x":6,"y":12,"size":2,"color":"text","text":"{time}"},
    {"type":"text","x":164,"y":16,"size":1,"align":"right","color":"dim","text":"{date.day} {date.md}"},
    {"type":"line","x":6,"y":38,"x2":164,"y2":38,"color":"dim"},
    {"type":"text","x":6,"y":48,"size":1,"color":"dim","text":"TARGETS"},
    {"type":"text","x":6,"y":64,"size":1,"color":"text","text":"{ping.0.name}"},
    {"type":"text","x":164,"y":64,"size":1,"align":"right","color":"{ping.0.color}","text":"{ping.0.ms} ms {ping.0.trend}"},
    {"type":"text","x":6,"y":80,"size":1,"color":"text","text":"{ping.1.name}"},
    {"type":"text","x":164,"y":80,"size":1,"align":"right","color":"{ping.1.color}","text":"{ping.1.ms} ms {ping.1.trend}"},
    {"type":"text","x":6,"y":96,"size":1,"color":"text","text":"{ping.2.name}"},
    {"type":"text","x":164,"y":96,"size":1,"align":"right","color":"{ping.2.color}","text":"{ping.2.ms} ms {ping.2.trend}"},
    {"type":"line","x":6,"y":116,"x2":164,"y2":116,"color":"dim"},
    {"type":"text","x":6,"y":126,"size":1,"color":"dim","text":"WIFI"},
    {"type":"text","x":164,"y":126,"size":1,"align":"right","color":"text","text":"{wifi.ssid}"},
    {"type":"bar","x":6,"y":142,"w":158,"h":8,"color":"{wifi.color}","value":"{wifi.pct}"},
    {"type":"text","x":164,"y":156,"size":1,"align":"right","color":"dim","text":"{wifi.rssi} dBm"},
    {"type":"line","x":6,"y":176,"x2":164,"y2":176,"color":"dim"},
    {"type":"text","x":6,"y":186,"size":1,"color":"dim","text":"SYSTEM"},
    {"type":"text","x":6,"y":202,"size":1,"color":"text","text":"{hostname}"},
    {"type":"text","x":164,"y":202,"size":1,"align":"right","color":"text","text":"{wifi.ip}"},
    {"type":"text","x":6,"y":218,"size":1,"color":"dim","text":"up {uptime}"},
    {"type":"text","x":164,"y":218,"size":1,"align":"right","color":"dim","text":"heap {heap}"}
  ]
})json";

static const char SIGNAL_METER[] = R"json({
  "name": "Signal Meter",
  "elements": [
    {"type":"text","x":85,"y":30,"size":1,"align":"center","color":"dim","text":"WIFI SIGNAL"},
    {"type":"text","x":85,"y":60,"size":4,"align":"center","color":"{wifi.color}","text":"{wifi.rssi}"},
    {"type":"text","x":85,"y":96,"size":1,"align":"center","color":"dim","text":"dBm"},
    {"type":"text","x":85,"y":120,"size":2,"align":"center","color":"{wifi.color}","text":"{wifi.bars}"},
    {"type":"bar","x":6,"y":156,"w":158,"h":12,"color":"{wifi.color}","value":"{wifi.pct}"},
    {"type":"text","x":164,"y":174,"size":1,"align":"right","color":"dim","text":"{wifi.pct}%"},
    {"type":"line","x":6,"y":200,"x2":164,"y2":200,"color":"dim"},
    {"type":"text","x":85,"y":214,"size":2,"align":"center","color":"text","text":"{wifi.ssid}"},
    {"type":"text","x":85,"y":240,"size":1,"align":"center","color":"dim","text":"{wifi.ip}"},
    {"type":"text","x":85,"y":304,"size":1,"align":"center","color":"dim","text":"{hostname}.local"}
  ]
})json";

static const char NETWORK[] = R"json({
  "name": "Network",
  "elements": [
    {"type":"text","x":6,"y":12,"size":1,"color":"dim","text":"HOSTNAME"},
    {"type":"text","x":6,"y":24,"size":2,"color":"text","text":"{hostname}"},
    {"type":"text","x":6,"y":56,"size":1,"color":"dim","text":"IP ADDRESS"},
    {"type":"text","x":2,"y":68,"size":2,"color":"text","text":"{wifi.ip}"},
    {"type":"text","x":6,"y":100,"size":1,"color":"dim","text":"NETWORK"},
    {"type":"text","x":6,"y":112,"size":2,"color":"text","text":"{wifi.ssid}"},
    {"type":"text","x":6,"y":144,"size":1,"color":"dim","text":"SIGNAL {wifi.rssi} dBm"},
    {"type":"bar","x":6,"y":158,"w":158,"h":10,"color":"{wifi.color}","value":"{wifi.pct}"},
    {"type":"line","x":6,"y":190,"x2":164,"y2":190,"color":"dim"},
    {"type":"text","x":6,"y":200,"size":1,"color":"dim","text":"UPTIME"},
    {"type":"text","x":164,"y":200,"size":1,"align":"right","color":"text","text":"{uptime}"},
    {"type":"text","x":6,"y":214,"size":1,"color":"dim","text":"FREE HEAP"},
    {"type":"text","x":164,"y":214,"size":1,"align":"right","color":"text","text":"{heap}"},
    {"type":"text","x":6,"y":228,"size":1,"color":"dim","text":"TIME"},
    {"type":"text","x":164,"y":228,"size":1,"align":"right","color":"text","text":"{time.sec}"},
    {"type":"text","x":85,"y":304,"size":1,"align":"center","color":"dim","text":"{date}"}
  ]
})json";

// Needs a data source called "weather" with fields temp, humidity and
// wind, e.g. Open-Meteo (see README, "Data sources"). Not preloaded
// because it shows "--" until that source exists.
static const char WEATHER[] = R"json({
  "name": "Weather",
  "elements": [
    {"type":"text","x":6,"y":8,"size":1,"color":"dim","text":"WEATHER"},
    {"type":"text","x":164,"y":8,"size":1,"align":"right","color":"{api.weather.color}","text":"{api.weather.age}"},
    {"type":"text","x":85,"y":56,"size":6,"align":"center","color":"text","text":"{api.weather.temp}"},
    {"type":"text","x":85,"y":112,"size":2,"align":"center","color":"dim","text":"deg C"},
    {"type":"line","x":6,"y":150,"x2":164,"y2":150,"color":"dim"},
    {"type":"text","x":6,"y":162,"size":1,"color":"dim","text":"HUMIDITY"},
    {"type":"text","x":164,"y":162,"size":1,"align":"right","color":"text","text":"{api.weather.humidity} %"},
    {"type":"text","x":6,"y":178,"size":1,"color":"dim","text":"WIND"},
    {"type":"text","x":164,"y":178,"size":1,"align":"right","color":"text","text":"{api.weather.wind} km/h"},
    {"type":"text","x":6,"y":194,"size":1,"color":"dim","text":"UPDATED"},
    {"type":"text","x":164,"y":194,"size":1,"align":"right","color":"text","text":"{api.weather.updated}"},
    {"type":"line","x":6,"y":222,"x2":164,"y2":222,"color":"dim"},
    {"type":"text","x":85,"y":246,"size":3,"align":"center","color":"text","text":"{time}"},
    {"type":"text","x":85,"y":304,"size":1,"align":"center","color":"dim","text":"{date.day} {date.md}"}
  ]
})json";

// Flow layout: no x/y, elements stack top to bottom inside boxes, with
// named styles, a ring gauge, status dots and a rounded card.
static const char CARDS[] = R"json({
  "name": "Cards",
  "style": {"direction":"column","gap":8,"padding":6},
  "styles": {
    "label": {"color":"dim"},
    "card":  {"background":"#101820","border":"#2a3a4a","radius":8,"padding":8,"gap":4},
    "big":   {"font":"bold","size":44,"align":"center"}
  },
  "elements": [
    {"type":"box","class":"card","children":[
      {"type":"text","class":"label","text":"TIME"},
      {"type":"text","class":"big","text":"{time}"},
      {"type":"text","text":"{date.day} {date.md}","font":"sans","size":14,"style":{"align":"center","color":"dim"}}
    ]},
    {"type":"box","class":"card","style":{"direction":"row","align":"center","gap":10},"children":[
      {"type":"arc","w":56,"value":"{wifi.pct}","color":"{wifi.color}","style":{"thickness":7}},
      {"type":"box","style":{"gap":2},"children":[
        {"type":"text","class":"label","text":"WIFI"},
        {"type":"text","text":"{wifi.rssi} dBm","size":2},
        {"type":"text","text":"{wifi.ssid}","color":"dim"}
      ]}
    ]},
    {"type":"box","class":"card","children":[
      {"type":"text","class":"label","text":"SERVERS 🖥"},
      {"type":"box","style":{"direction":"row","align":"center","gap":6},"children":[
        {"type":"circle","w":10,"color":"{ping.0.color}"},
        {"type":"text","text":"{ping.0.name}"},
        {"type":"text","text":"{ping.0.ms} ms","style":{"align":"right"}}
      ]},
      {"type":"box","style":{"direction":"row","align":"center","gap":6},"children":[
        {"type":"circle","w":10,"color":"{ping.1.color}"},
        {"type":"text","text":"{ping.1.name}"},
        {"type":"text","text":"{ping.1.ms} ms","style":{"align":"right"}}
      ]},
      {"type":"box","style":{"direction":"row","align":"center","gap":6},"children":[
        {"type":"circle","w":10,"color":"{ping.2.color}"},
        {"type":"text","text":"{ping.2.name}"},
        {"type":"text","text":"{ping.2.ms} ms","style":{"align":"right"}}
      ]}
    ]},
    {"type":"box","style":{"direction":"row","justify":"between","align":"center"},"children":[
      {"type":"triangle","points":[[0,8],[5,0],[10,8]],"color":"accent"},
      {"type":"text","text":"up {uptime}","color":"dim"},
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
