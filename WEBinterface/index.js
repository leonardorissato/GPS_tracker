console.log("JS running");

let supabasePublicKey = localStorage.getItem("key");
if (!supabasePublicKey) {
  document.getElementById("settingswrapper").style.display = "grid";
}

const { createClient } = supabase;
//const bridgeDBInstance = createClient(bridgeDB_URL, supabasePublicKey);
const DBInstance = createClient("https://pllwfhcjryzuqsprjdqs.supabase.co", supabasePublicKey, {
  realtime: {
    params: {
      eventsPerSecond: 1,
    },
  },
});

bridgeDBValues = {
  realtime: null,
  ignitionlock: null,
  batterylevel: null,
  lastupdate: null,
  gpsstatus: null,
};

let autoCentralize = true;

let locationDBData = [];
let bikePosition = null;
let lastupdatetime = null;
let lastbatterylevel = null;

let timeframeoffset = 24;

// Subscribe to changes on the database
DBInstance.channel("supabase_realtime")
  .on("postgres_changes", { event: "*", schema: "*" }, (payload) => {
    if (payload.table == "locationHistory") {
      // PAYLOAD RECEIVED FROM DATABASE
      //   {
      //     "schema": "public",
      //     "table": "locationHistory",
      //     "commit_timestamp": "2025-03-18T06:40:13.278Z",
      //     "eventType": "INSERT",
      //     "new": {
      //         "acc": 99,
      //         "created_at": "2025-03-18T06:40:13.277034+00:00",
      //         "id": 888,
      //         "ign": true,
      //         "lat": -37,
      //         "localtime": "jacare",
      //         "lon": 145,
      //         "spd": 99
      //     },
      //     "old": {},
      //     "errors": null
      // }
      console.log("change on locationhistory");
      locationDBData.push(payload.new);
      bikePosition = locationDBData[locationDBData.length - 1];
      sortDataArray();
      updateInterface();
    }
    if (payload.table == "bridgeDB") {
      // PAYLOAD RECEIVED FROM DATABASE
      //   {
      //     "schema": "public",
      //     "table": "bridgeDB",
      //     "commit_timestamp": "2025-03-18T09:49:29.282Z",
      //     "eventType": "UPDATE",
      //     "new": {
      //         "id": 46,
      //         "value": "off",
      //         "variable": "gpsstatus"
      //     },
      //     "old": {
      //         "id": 46
      //     },
      //     "errors": null
      // }

      console.log("change on bridgeDB");

      bridgeDBValues[payload.new.variable] = payload.new.value;

      updateInterface();
    }
  })
  .subscribe();

// MAPS
var map = L.map("map").setView([0, 0], 14);
L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
  maxZoom: 19,
  attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>',
}).addTo(map);

getLocationData();
getBridgeDBValues();

//=================== UPDATE BRIDGE VALUES ==================================
async function updateBridgeDB(variablename, value) {
  const req = ({ data, error } = await bridgeDBInstance.from("bridgeDB").update({ value: value }).eq("variable", variablename).select());

  return data;
}

//=================== GET LOCATION VALUES ==================================
async function getLocationData() {
  // time format  = "2024-11-23 08:03:50.998074+00"
  // other variables
  let time = new Date();
  let timeframeStart = new Date(time.setHours(time.getHours() - 96)).toISOString().replace("T", " ").replace("Z", "+00");
  const { data, error } = await DBInstance.from("locationHistory").select().gte("created_at", timeframeStart);

  // return if empty
  if (data.length < 1) {
    console.log("position array is empty");
    return;
  }

  // sort elements by utc time? maybe not, it comes sorted I think
  // data.forEach((element) => {
  //   locationDBData.push({ lat: element.lat, lon: element.lon, localtime: element.localtime, created_at: element.created_at });
  // });

  locationDBData = data;
  sortDataArray();

  updateInterface();
}

async function getBridgeDBValues() {
  let req = ({ data: bridgeDB, error } = await DBInstance.from("bridgeDB").select("*"));

  if (!req.data) {
    console.log("error on bridgeDB");
    return;
  }

  bridgeDBValues.realtime = req.data.find((e) => e.variable == "realtime").value;
  bridgeDBValues.ignitionlock = req.data.find((e) => e.variable == "ignitionlock").value;
  bridgeDBValues.batterylevel = req.data.find((e) => e.variable == "batterylevel").value;
  bridgeDBValues.lastupdate = req.data.find((e) => e.variable == "lastupdate").value;
  bridgeDBValues.gpsstatus = req.data.find((e) => e.variable == "gpsstatus").value;

  updateInterface();
}

function sortDataArray() {
  locationDBData.sort(function (a, b) {
    return a.id - b.id;
  });
}

//=================== TIME FRAME FOR DATA VIEW ==================================
function setTimeFrameOffset(value) {
  console.log("timeframe = ", value);
  timeframeoffset = value;

  updateInterface();
}

function centralizemap() {
  // update map center to the last position, keep zoom value, set pan animation
  map.flyTo(bikePosition, map._zoom, { animate: true, duration: 0.8 });
}

let clickcapybara = 0;
function settingsclick() {
  console.log("clicked on settings");
  if (clickcapybara++ > 3) {
    document.getElementById("settingswrapper").style.display = "grid";
  }
}

function settingssubmited() {
  console.log("submit");
  let value = document.getElementById("settingstextinput").value;
  localStorage.setItem("key", value);

  supabasePublicKey = localStorage.getItem("key");
}

let mapPolyline = null;
let mapMarker = null;
let mapAccuracy = null;

function updateInterface() {
  // update map if we have data available from server
  if (locationDBData.length > 0) {
    // clear lines and markers if they were already instantiated
    if (mapPolyline) {
      map.removeLayer(mapPolyline);
    }
    if (mapMarker) {
      map.removeLayer(mapMarker);
    }
    if (mapAccuracy) {
      map.removeLayer(mapAccuracy);
    }

    // filter location data
    let timeframeStart = new Date(new Date().setHours(new Date().getHours() - timeframeoffset));
    let filteredData = locationDBData.filter((d) => {
      return new Date(d.created_at).getTime() >= timeframeStart.getTime();
    });

    let polylinePoints = [];

    if (filteredData.length > 0) {
      filteredData.forEach((element) => {
        polylinePoints.push([element.lat, element.lon]);
      });

      // add points to map, filter by timeframe
      mapPolyline = L.polyline(polylinePoints, {
        color: "#bd0026",
        weight: 2,
        opacity: 0.8,
        smoothFactor: 1.1,
      }).addTo(map);

      // if we want to add anything to the polilyne later
      //polyline.addLatLng([-38.114, 145.763]);
    }

    // add a marker to the last available position
    bikePosition = locationDBData[locationDBData.length - 1];

    let date = bikePosition.localtime.split(",")[0];
    let time = bikePosition.localtime.split(",")[1];

    let dateparts = date.split("/");
    let datetext = new Date("20" + dateparts[0], dateparts[1] - 1, dateparts[2]);

    let timeparts = time.split(":");
    let timetext = timeparts[0] + ":" + timeparts[1];

    let popuptext = datetext.toDateString() + " - " + timetext + " - " + bikePosition.acc + "m";
    mapMarker = L.marker(bikePosition).addTo(map);
    //mapMarker.bindPopup(popuptext).openPopup();
    mapMarker.bindTooltip(popuptext).openTooltip();

    let accuracy = bikePosition.acc < 5 ? 5 : bikePosition.acc;
    mapAccuracy = L.circle(bikePosition, { radius: accuracy, color: "#000000", weight: 1, opacity: 0.8 }).addTo(map);

    centralizemap();
  }

  updateIconsAndText();
}

setInterval(() => {
  updateIconsAndText();
}, 5000);
function updateIconsAndText() {
  if (!bridgeDBValues.lastupdate) {
    console.log("Still don't have values from bridgedb");
    return;
  }

  // date info from bridgeDB
  // date format : "25/03/28,19:40:49+44";
  let date = bridgeDBValues.lastupdate.split(",")[0];
  let time = bridgeDBValues.lastupdate.replace("+", ",").split(",")[1]; // replace the + symbol so I can get the minutes alone

  let dateparts = date.split("/");
  let timeparts = time.split(":");

  let lastUpdateDate = new Date("20" + dateparts[0], dateparts[1] - 1, dateparts[2], timeparts[0], timeparts[1], timeparts[2]);

  // update time on interface
  let lastupdatetime = lastUpdateDate.toLocaleString("pt-BR");
  document.getElementById("lastupdate").innerText = lastupdatetime;

  //update GPS and network icons
  let gpsicon = document.getElementById("gps_icon");
  let networkicon = document.getElementById("network_icon");

  let minutediff = (new Date() - lastUpdateDate) / 1000 / 60;

  if (minutediff > 5) {
    // if we're more than 5 minutes outdated, show as off
    networkicon.src = "./network_off.png";
    if (bridgeDBValues.gpsstatus == "on") {
      gpsicon.src = "./gps_time.png"; // gps was working, it's only outdated
    } else {
      gpsicon.src = "./gps_off.png"; // gps was not working at the time of update
    }
  } else {
    // if we're less than 5 minutes, show network on
    networkicon.src = "./network_on.png";
    // check gps status coming from tracker
    if (bridgeDBValues.gpsstatus == "on") {
      gpsicon.src = "./gps_on.png"; // gps is working right now
    } else {
      gpsicon.src = "./gps_off.png"; // gps is not working right now
    }
  }

  let batterytext = isNaN(parseInt(bridgeDBValues.batterylevel)) ? "Charging" : String((parseInt(bridgeDBValues.batterylevel) / 1000).toFixed(1)) + "V";
  document.getElementById("batteryvoltage").innerText = batterytext;
}
