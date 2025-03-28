console.log("JS running");

const locationDB_URL = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/locationHistory?&select=*";
const bridgeDB_URL = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/bridgeDB?&select=*";
let test = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBsbHdmaGNqcnl6dXFzcHJqZHFzIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MzA3OTA5NTUsImV4cCI6MjA0NjM2Njk1NX0.-tO3afiuTGl_CHUPSvhdOq0Na1sY5SpeENhplWg5r9U";

let supabasePublicKey = localStorage.getItem("key");
if (!supabasePublicKey) {
  document.getElementById("settingswrapper").style.display = "grid";
}

const { createClient } = supabase;
const bridgeDBInstance = createClient(bridgeDB_URL, supabasePublicKey);
const locationDBInstance = createClient(locationDB_URL, supabasePublicKey);

// columns names: realtime / ignitionlock
const bridge_realtime = "realtime";
const bridge_ignitionlock = "ignitionlock";
const bridge_batterylevel = "batterylevel";
const bridge_lastupdate = "lastupdate";
const bridge_gpsstatus = "gpsstatus";

// other variables
let timestartframe;
let timeendframe;
let timeframeoffset = 24;
let polilynepoints = [];
let bikeposition = null;
let lastupdatetime = null;
let lastbatterylevel = null;

// updateBridgeDB(realtime, "false").then(
//   function (value) {
//     console.log("VARIABLE UPDATED");
//   },
//   function (error) {
//     console.log("ERROR UPDATING VARIABLE");
//     console.error(error);
//   }
// );

// MAPS
var map = L.map("map").setView([0, 0], 10);
L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
  maxZoom: 19,
  attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>',
}).addTo(map);

// // UPDATE INTERFACE EVERY SECOND
// setInterval(() => {
//   // new times
//   updatetimeframe();

//   // get batterylevel
//   getBridgeDBValue(bridge_batterylevel).then(
//     function (value) {
//       lastbatterylevel = value;
//     },
//     function (error) {
//       console.log("ERROR GETTING BATTERY LEVEL");
//       console.error(error);
//     }
//   );

//   // get last updated time
//   getBridgeDBValue(bridge_lastupdate).then(
//     function (value) {
//       lastupdatetime = String(value).slice(0, 14).replace(",", " - ");
//     },
//     function (error) {
//       console.log("ERROR GETTING BATTERY LEVEL");
//       console.error(error);
//     }
//   );

//   // get gps status
//   getBridgeDBValue(bridge_gpsstatus).then(
//     function (status) {
//       let statusdisplay = document.getElementById("gpsstatus");
//       if (status == "off") {
//         statusdisplay.innerText = "GPS OFF";
//         statusdisplay.style.color = "red";
//       } else if (status == "on") {
//         statusdisplay.innerText = "GPS ON";
//         statusdisplay.style.color = "green";
//       } else {
//         statusdisplay.innerText = "WTF???";
//         statusdisplay.style.color = "lightblue";
//       }
//     },
//     function (error) {
//       console.log("ERROR GETTING BATTERY LEVEL");
//       console.error(error);
//     }
//   );

//   // retrieve new data
//   getLocationData(timestartframe, timeendframe).then(
//     function (arraypositions) {
//       //console.log(arraypositions);

//       // clear map
//       map.eachLayer((layer) => {
//         if (layer instanceof L.Marker) {
//           layer.remove();
//         }
//       });

//       // return if empty
//       if (arraypositions.length < 1) {
//         console.log("position array is empty");
//         return;
//       }

//       // sort elements by utc time?

//       arraypositions.forEach((element) => {
//         polilynepoints.push([element.lat, element.lon]);
//       });
//       // add points to map
//       var polyline = L.polyline(polilynepoints, {
//         color: "#bd0026",
//         weight: 2,
//         opacity: 0.8,
//         smoothFactor: 1.1,
//       }).addTo(map);

//       // if we want to add anything to the polilyne later
//       //polyline.addLatLng([-37.114, 145.763]);

//       // update center position of the map if it's the first time I opened the page
//       if (!bikeposition) {
//         bikeposition = polilynepoints[polilynepoints.length - 1];
//         centralizemap();
//       }

//       // add a marker to the last element
//       bikeposition = polilynepoints[polilynepoints.length - 1];
//       var marker = L.marker(bikeposition).addTo(map);
//     },
//     function (error) {
//       console.log("ERROR UPDATING VARIABLE");
//       console.error(error);
//     }
//   );

//   let batterytext = isNaN(parseInt(lastbatterylevel)) ? "Charging" : String((parseInt(lastbatterylevel) / 1000).toFixed(1)) + "V";

//   writelastupdate(lastupdatetime + " - " + batterytext);
// }, 1000);

const jacare = createClient("https://pllwfhcjryzuqsprjdqs.supabase.co", supabasePublicKey, {
  realtime: {
    params: {
      eventsPerSecond: 10,
    },
  },
});
jacare
  .channel("room1")
  .on("postgres_changes", { event: "INSERT", schema: "public", table: "locationHistory" }, (payload) => {
    console.log("Change on Database!!!", payload);
  })
  .subscribe();

//================ GET BRIDGE VALUES ====================================
async function getBridgeDBValue(variablename) {
  let req = ({ data: bridgeDB, error } = await bridgeDBInstance.from("bridgeDB").select("*").eq("variable", variablename));
  if (req.data) {
    return req.data[0].value;
  } else {
    return null;
  }
}

//=================== UPDATE BRIDGE VALUES ==================================
async function updateBridgeDB(variablename, value) {
  const req = ({ data, error } = await bridgeDBInstance.from("bridgeDB").update({ value: value }).eq("variable", variablename).select());

  return data;
}

//=================== GET LOCATION VALUES ==================================
async function getLocationData(startTime, endTime) {
  // time format  = "2024-11-23 08:03:50.998074+00"
  const { data, error } = await locationDBInstance.from("locationHistory").select().gte("created_at", startTime).lte("created_at", endTime);

  return data;
}

//=================== TIME FRAME FOR DATA VIEW ==================================
function updatetimeframe() {
  let time = new Date();
  timeendframe = time.toISOString().replace("T", " ").replace("Z", "+00");
  timestartframe = new Date(time.setHours(time.getHours() - timeframeoffset)).toISOString().replace("T", " ").replace("Z", "+00");
}

function setTimeFrameOffset(value) {
  console.log("timeframe = ", value);
  timeframeoffset = value;
}

function centralizemap() {
  console.log("centralizing map");
  // update map center to the last position
  map.setView(bikePosition, 15);
}

function writelastupdate(value) {
  document.getElementById("lastupdate").innerText = value;
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
