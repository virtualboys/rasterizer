var five = require("johnny-five");
var board = new five.Board();
const { Client } = require("node-osc");

const client = new Client("127.0.0.1", 1234);

function transformVal(val) {
	return (val / 512);
}

board.on("ready", function() {
  // Create an Led on pin 13
  var ctrlA = five.Sensor({
    pin: "A0",
    freq: 5
  });
  var ctrlB = five.Sensor({
    pin: "A1",
    freq: 5
  });
  var ctrlC = five.Sensor({
    pin: "A2",
    freq: 5
  });
  
  ctrlA.on("data", value => {
    client.send("/cv/A", transformVal(value))
  });

  ctrlB.on("data", value => {
    client.send("/cv/B", transformVal(value))
    
    
  });

  ctrlC.on("data", value => {
    client.send("/cv/C", transformVal(value))
    console.log('valueA ', transformVal(value))
    //console.log('valueC ', value)
  });
  /*ctrlB.on("data", value => {
    //console.log("  value  : ", ctrlB.value);
    client.send("/B", ctrlB.value, () => {
      //console.log("sent");
    });
  });*/
  /*ctrlC.on("data", value => {
    //console.log("  value  : ", ctrlC.value);
    client.send("/C", ctrlC.value, () => {
      //console.log("sent");
    });
  });*/ 
});
