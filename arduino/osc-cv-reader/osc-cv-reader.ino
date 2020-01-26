#include <SLIPEncodedSerial.h>
#include <OSCData.h>
#include <OSCBundle.h>
#include <OSCBoards.h>
#include <OSCTiming.h>
#include <OSCMessage.h>
#include <OSCMatch.h>
#include <SLIPEncodedUSBSerial.h>


/*
    Make an OSC bundle and send it over SLIP serial

    OSCBundles allow OSCMessages to be grouped together to  preserve the order and completeness of related messages.
    They also allow for timetags to be carried to represent the presentation time of the messages.
*/
#include <OSCBundle.h>
#include <OSCBoards.h>

#ifdef BOARD_HAS_USB_SERIAL
#include <SLIPEncodedUSBSerial.h>
SLIPEncodedUSBSerial SLIPSerial( thisBoardsSerialUSB );
#else
#include <SLIPEncodedSerial.h>
 SLIPEncodedSerial SLIPSerial(Serial); // Change to Serial1 or Serial2 etc. for boards with multiple serial ports that don’t have Serial
#endif


void setup() {
  //begin SLIPSerial just like Serial
    SLIPSerial.begin(9600);   // set this as high as you can reliably run on your platform
}

int val = 0;
void loop(){
    //declare the bundle
    OSCBundle bndl;
    //BOSCBundle's add' returns the OSCMessage so the message's 'add' can be composed together
    bndl.add("/analog/0").add((int32_t)val);
//    bndl.add("/analog/1").add((float)val);
//    bndl.add("/string/5").add("hello");

    val += 1;
    if(val > 800) {
      val = 0;
    }


    SLIPSerial.beginPacket();
        bndl.send(SLIPSerial); // send the bytes to the SLIP stream
    SLIPSerial.endPacket(); // mark the end of the OSC Packet
    bndl.empty(); // empty the bundle to free room for a new one

    delay(100);
}
