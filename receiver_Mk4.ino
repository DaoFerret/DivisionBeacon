// Shoulder beacon receiver code
// running on FioV3 arduino based board w/XBee s1, Adafruit 16xNeoPixel, & AdaFruit Soundboard
// By: DaoFerret
// Designed as a pair with transmitter code, which transmits button presses to the receiver.
// Several effects were adapted from Shoulder beacon code by (sorry I couldn't find the developer
// I borrowed ... they also makde the 3D design files that I worked off of, so when I find it, I'll
// update everything) from which was also adapted from the Adafruit NeoPixel goggle example code.

#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include "Adafruit_Soundboard.h"

/* constants/variables used for the NeoPixel Ring

*/
#define PIN 3
#define NUM_LEDS 16
#define BRIGHTNESS 50

// Inistatiate the object for dealing with the NeoPixel ring
Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB);

/* AdaFruit SoundBoard

    Setup the Interface to the SoundBoard
   Choose any two pins that can be used with SoftwareSerial to RX & TX
*/
// pins for the Serial interface to the SoundBoard
#define SFX_TX 5  // Pin for the TX to the SoundBoard Interface
#define SFX_RX 6  // Pin for the RX to the SoundBoard Interface
#define SFX_RST 7 // Pin to trigger the reset pulldown on the SoundBoard
#define SFX_ACT 8 // Pin to monitor when the Soundboard is playing audio
#define SPKR_OFF 2 // Pin to turn the Speaker off

// Define the software serial interface to the soundboard
SoftwareSerial ss = SoftwareSerial(SFX_TX, SFX_RX);
// pass the software serial to Adafruit_soundboard, the second
// argument is the debug port (not used really) and the third
// arg is the reset pin
Adafruit_Soundboard sfx = Adafruit_Soundboard(&ss, NULL, SFX_RST);


// Constants/Vars used for command receiving/processing from receiver
String input_string = "";         // a string to hold incoming data
boolean string_complete = false;  // whether the string is complete
int received_button_press = 0;


/*
   Variables used to keep track of the State of the system
*/
#define BASE_STATE     0     // 00-xx-xx
#define DARK_ZONE      10000 // 01-xx-xx
#define GONE_ROGUE       100 // xx-01-xx
#define FUN_ZONE       20000 // 02-xx-xx
#define SINGLE_TRACK     100 // xx-01-xx
#define PLAYLIST         200 // xx-02-xx
#define SETTINGS_ZONE  30000 // 03-xx-xx
#define BASE_SUB_STATE 0     // xx-00-xx

int team_count = 0;  // number of other members in your group
int current_state = BASE_STATE;           // Used to track the current state
int current_sub_state = BASE_SUB_STATE;   // Used to track the current sub-state
bool current_state_first_run = false;     // Used to track if this is the first time in the state
//    so we can trigger things like audio, as opposed to
//    ongoing NeoPixel effects.
bool first_rogue_state = true;            // Keep track of the first time you go Rogue for DARK_ZONE/GONE_ROGUE mode

uint16_t current_track = 0;               // Keep track of current track number for FUN_ZONE SINGLE_TRACK mode
uint16_t current_playlist = 0;            // Keep track of current playlist number for FUN_ZONE PLAYLIST mode
bool repeat_play = false;                 // By default repeat is off
bool random_play = false;                 // By default random play is off
#define total_tracks 14                   // Total tracks loaded
#define total_playlists 1                 // Total playlists loaded


bool disco_mode = false;                  // just start having "fun"
uint8_t current_effect = 0;
int current_effect_count = 0;
bool fz_standard_throbber = false;        // just do the standard pulse while playing songs

// Variables needed to keep state in the NeoPixel effects between runs
uint32_t throb_brightness = 85; // Current throb brightness
uint8_t offset = 0; // Current position of spinner

void setup() {
  // put your setup code here, to run once:

  // Self monitoring pins
  pinMode(11, INPUT);
  pinMode(13, INPUT); // Battery monitoring

  /* formula for battery voltage (cribbed from sparkfun product page comment)
      It appears that the VBATT_LVL pin is not correctly marked.
      To monitor the voltage from the battery you will need to use A11.
      You can use the following formula to determine the actual input voltage.
      (sampleValue * (3.3/1024)) * 2
      The sample value is the value from the analogRead(A11) call
      3.3/1024 obtains the number of mV/step. The DAC is 10 bit,
      so there are 1024 values available and the processor is operating at 3.3v.
      ½ the input voltage to the divider is sent to the pin per this wiki entry:
      http://en.wikipedia.org/wiki/Voltage_divider
    So in this case we multiply by 2 to reverse the division.*/

  // initialize the PixelRing
  ring.setBrightness(BRIGHTNESS);
  ring.begin();
  ring.show(); // Initialize all pixels to 'off'

  // Open a connection for debugging
  Serial.begin(9600);

  // Open a connection to the XBee
  Serial1.begin(9600);

  // Open a connection to the Soundboard
  // softwareserial at 9600 baud
  // wait a few seconds ... this is one of those connections that I believe I can get to work.
  //  delay(3000);
  ss.begin(9600);
  if (!sfx.reset()) {
    //    Serial.println("Not found");
    //    while (1);
  } else {
    // (only set the pin to "input" if we have the soundboard turned on)
    // Pin for monitoring the Soundboard "play" state (should be active if the board is playing a sound effect)
    pinMode(SFX_ACT, INPUT);
  }
  //  Serial.println("SFX board found");

  // Raise Soundboard Volume to max
  sfx.volUp();
  sfx.volUp();
  sfx.volUp();
  sfx.volUp();
  sfx.volUp();

  // initialize the Random-ness
  randomSeed(analogRead(0));

  // reserve 200 bytes for the input_string:
  // (WAY more than enough considerng we are sending two byte messages from the transmitter)
  input_string.reserve(200);

  state_startup();
}

void loop() {
  // put your main code here, to run repeatedly:

  // print the string when a newline arrives:
  // When the string is complete, push the state
  if (string_complete) {
    Serial.println(input_string);
    // clear the string:
    received_button_press = input_string.toInt();
    input_string = "";
    string_complete = false;
  }

  /*  Test Code to change the NeoPixel ring to the same color as the button press
    // Switches to compute new state
    if (received_button_press != 0) {
      switch ( received_button_press ) {
        case 1:
          colorWipe(ring.Color(125, 0, 125), 50); // Black (purple)
          break;
        case 2:
          colorWipe(ring.Color(0, 0, 125), 50); // Blue
          break;
        case 3:
          colorWipe(ring.Color(0, 125, 0), 50); // Green
          break;
        case 4:
          colorWipe(ring.Color(125, 0, 0), 50); // Red
          break;
        case 5:
          colorWipe(ring.Color(125, 125, 125), 50); // White
          break;
      }
    }
  */

  /* Code to determine the new State:
     If we receive a button press, we want to concatenate them with the
     State and SubState to produce a lookup for the new state based on the current one
     and the buttons.  Since Arduino doesn't (by default) have hashes/associative array,
     we "fake it" by having each decimal place.
     00     00        00
     State  Substate  received_button_press
     (this lets us support up to 99 States, Substates, and button presses ...
      ... way more than I'm hoping I need in the immediate future, so I don't
      have to refactor this solution in the near term :) )

     So the default state is "0"

     List of defined States/Substates:
     000000 = Base state (throb)
     010000 = Dark Zone  (throb)
     010100 = Dark Zone/Gone Rogue (throb red)
     020000 = Fun "Zone" (throb blue)
     030000 = Settings "zone"page (throb white)

  */
  if (received_button_press != 0) {
    uint32_t switch_state = (current_state + current_sub_state + received_button_press);
    received_button_press = 0;
    switch (switch_state) {
      case 1: // 00-00-01 Base State --> Dark Zone
        current_state = DARK_ZONE;
        current_sub_state = BASE_SUB_STATE;
        current_state_first_run = true;
        break;

      case 2: // 00-00-02 Base State --> Fun Zone
        current_state = FUN_ZONE;
        current_sub_state = BASE_SUB_STATE;
        current_state_first_run = true;
        break;

      case 3: // 00-00-03 Base State --> Base State (Add Team Member)
        add_team_member();
        break;

      case 4: // 00-00-04 Base State --> Base State (Del Team Member)
        del_team_member();
        break;

      case 5: // 00-00-05 Base State --> World Events
        play_world_event();
        break;

      case 10001: // 01-00-01 Dark Zone --> Base State (leave dark zone)
        current_state = BASE_STATE;
        current_sub_state = BASE_SUB_STATE;
        current_state_first_run = true;
        first_rogue_state = true;
        dark_zone_pulse_last_run();
        break;

      case 10002: // 01-00-02 Dark Zone --> Fire off Pulse
      case 10102: // 01-01-02 DZ/Gone Rogue --> Fire off Pulse
        fire_a_pulse();
        break;

      case 10003: // 01-00-03 Dark Zone --> Gone Rogue (solo/team)
      case 10103: // 01-01-03 DZ/Gone Rogue --> killed another one
        current_state = DARK_ZONE;
        current_sub_state = GONE_ROGUE;
        current_state_first_run = true;
        break;

      case 10004: // 01-00-04 Dark Zone --> Kill Rogue
      case 10104: // 01-01-04 DZ/Gone Rogue --> Kill Rogue
        kill_a_rogue();
        break;

      case 10101: // 01-01-01 DZ/Gone Rogue --> attempt to exit DZ
        exit_DZ_while_rogue();
        break;

      case 10105: // 01-01-05 DZ/Gone Rogue --> Dark Zone (end Rogue status)
        current_state = DARK_ZONE;
        current_sub_state = BASE_STATE;
        end_rogue_status();
        break;

      case 20001: // 02-00-01 Fun Zone --> Base State
        current_state = BASE_STATE;
        current_sub_state = BASE_SUB_STATE;
        current_state_first_run = true;
        break;

      case 20002: // 02-00-02 Fun Zone --> FZ/Single Track mode
        current_state = FUN_ZONE;
        current_sub_state = SINGLE_TRACK;
        current_state_first_run = true;
        current_track = 0;
        break;

      case 20003: // 02-00-03 Fun Zone --> toggle Disco mode for the throbber
        // (changes the default Fun Zone throbber to a random effect disco ball)
        toggle_disco_mode();
        break;

      case 20004: // 02-00-04 Fun Zone --> toggle Maintain standard mode
        // (changes the track specific throbber to be the "standard" color one)
        toggle_fz_standard_throbber();
        break;

      case 20101: // 02-01-01 FZ/Single Track mode --> toggle repeat mode
        single_track_repeat();
        break;

      case 20102: // 02-01-02 FZ/Single Track mode --> FZ/Playlist mode
        current_state = FUN_ZONE;
        current_sub_state = PLAYLIST;
        current_state_first_run = true;
        current_track = 0;
        current_playlist = 0;
        break;

      case 20103: // 02-01-03 FZ/Sing Track mode : next track
        single_track_next();
        break;

      case 20104: // 02-01-04 FZ/Sing Track mode : prev track
        single_track_prev();
        break;

      case 20105: // 02-01-05 FZ/Sing Track mode : stop/play
        single_track_stop_play();
        break;

      //      case 20201: // 02-02-01 FZ/Playlist mode --> toggle Random next track
      //        playlist_random();
      //        break;

      case 20202: // 02-02-02 FZ/Playlist mode --> Fun Zone
        current_state = FUN_ZONE;
        current_sub_state = BASE_SUB_STATE;
        current_state_first_run = true;
        break;

      case 20005: // 02-00-05 Fun Zone --> Settings Zone
        current_state = SETTINGS_ZONE;
        current_sub_state = BASE_SUB_STATE;
        current_state_first_run = true;
        break;

      case 30001: // 03-00-01 Settings Zone --> Base State
        current_state = BASE_STATE;
        current_sub_state = BASE_SUB_STATE;
        current_state_first_run = true;
        break;

    }
  }

  // Switches to subroutines to do something based on the current state
  switch ( current_state ) {
    case BASE_STATE: // default main mode
      switch ( current_sub_state ) {
        case BASE_SUB_STATE: //default "pulse and wait"
          if (current_state_first_run) {
            state_pulse_first_run();
            current_state_first_run = false;
          }
          state_pulse();
          break;
      }
      break; // end BASE_STATE

    case DARK_ZONE:
      switch ( current_sub_state ) {
        case BASE_SUB_STATE:
          if (current_state_first_run) {
            dark_zone_pulse_first_run();
            current_state_first_run = false;
          }
          dark_zone_pulse();
          break;

        case GONE_ROGUE:
          if (current_state_first_run) {
            gone_rogue_pulse_first_run();
            current_state_first_run = false;
          }
          gone_rogue_pulse();
      }
      break; // end DARK_ZONE

    case FUN_ZONE:
      switch ( current_sub_state ) {
        case BASE_SUB_STATE:
          if (current_state_first_run) {
            fun_zone_first_run();
            current_state_first_run = false;
          }
          fun_zone_pulse();
          break;

        case SINGLE_TRACK:
          if (current_state_first_run) {
            single_track_first_run();
            current_state_first_run = false;
          }
          single_track_pulse();
          break;

        case PLAYLIST:
          if (current_state_first_run) {
            playlist_first_run();
            current_state_first_run = false;
          }
          playlist_pulse();
          break;
      }
      break; // end FUN_ZONE

    case SETTINGS_ZONE:
      switch (current_sub_state ) {
        case BASE_SUB_STATE:
          if (current_state_first_run) {
            settings_zone_first_run();
            current_state_first_run = false;
          }
          settings_zone_pulse();
          break;
      }
      break; // end SETTINGS_ZONE
  }

}


/* Startup subroutine
    - Trigger the "ISAC coming online" sound effect
    - Bring the Beacon "up"
*/
void state_startup() {
  // Play startup track
  char sfx_trackname[20] = "ISACBOOTOGG\n";
  sfx.playTrack(sfx_trackname);

  // Green "ladder chase"
  chaseUpDown(50, 0x000000, 0x00FF00, 4);

  // Green Sparks
  randomSpark (50, 0x00FF00, BRIGHTNESS, 20);

  // Amber Sparks
  //randomSpark (50, 0x994C00, BRIGHTNESS, 32);
  //randomSpark (50, 0xFFC300, BRIGHTNESS, 32);
  //randomSpark (50, 0xFF9933, BRIGHTNESS, 32);
  randomSpark (50, 0xFF6600, BRIGHTNESS, 32);

  // Random wipe to Amber
  //randomFill (50, 0x994C00, BRIGHTNESS);
  //randomFill (50, 0xFFC300, BRIGHTNESS);
  //randomFill (50, 0xFF9933, BRIGHTNESS);
  randomFill (50, 0xFF6600, BRIGHTNESS);

  // will settle into "standard" Amber pulse after this
}

// NORMAL STATE
void state_pulse_first_run() {
  char sfx_trackname[20];
  String("ALERTUP1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
  sfx.playTrack(sfx_trackname);
  //color_transition(7, 0x994C00);
  //color_transition(7, 0xFFC300);
  //color_transition(7, 0xFF9933);
  color_transition(7, 0xFF6600);
}

void state_pulse() {
  //pulse(7, 0x994C00, 20, 180);
  //pulse(7, 0xFFC300, 20, 180);
  //pulse(7, 0xFF9933, 20, 180);
  pulse(7, 0xFF6600, 20, 180);
}


// Add a team member, or play a "team member down" sound effect if we're over 3 additional
// team members.
void add_team_member() {
  char sfx_trackname[20];
  int8_t sfx_count = random(3);

  // if we have under 3 team members then add another one
  if (team_count < 3) {
    team_count++;
    switch (sfx_count) {
      case 0:
        String("AJOINT00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 1:
        String("AJOINT01OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 2:
        String("AJOINT02OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
    }

    // if we're playing a different sound, we pre-empt it.
    if (digitalRead(SFX_ACT) == LOW) {
      sfx.stop();
    }

    sfx.playTrack(sfx_trackname);

    // Color pop Green
    colorPop(0x00FF00, 75, 5);

    // we have more than three team members, so play a "team member down" sound effect instead.
  } else {
    switch (sfx_count) {
      case 0:
        String("FADOWN00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 1:
        String("FADOWN01OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 2:
        String("FADOWN02OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
    }

    // if we're playing a different sound, we pre-empt it.
    if (digitalRead(SFX_ACT) == LOW) {
      sfx.stop();
    }

    sfx.playTrack(sfx_trackname);

    // Color pop Red
    while (digitalRead(SFX_ACT) == LOW) {
      colorPop(0xFF0000, 50, 5);
    }
  }
}

void del_team_member() {
  char sfx_trackname[20];
  int8_t sfx_count = random(3);

  // if we have under 3 team members then add another one
  if (team_count > 0) {
    team_count--;
    switch (sfx_count) {
      case 0:
        String("ALEAVE00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 1:
        String("ALEAVE01OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 2:
        String("ALEAVE02OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
    }

    // if we're playing a different sound, we pre-empt it.
    if (digitalRead(SFX_ACT) == LOW) {
      sfx.stop();
    }

    sfx.playTrack(sfx_trackname);

    // Color pop Green
    colorPop(0x00FF00, 75, 5);

    // we have more no team members, so ... ???
  }
}

void play_world_event() {
  char sfx_trackname[20];
  int8_t sfx_count = random(4);

  switch (sfx_count) {
    case 0:
      String("WEVENT00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 1:
      String("WEVENT01OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 2:
      String("WEVENT02OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 3:
      String("WEVENT03OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
  }

  // if we're playing a different sound, we pre-empt it.
  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  // Chase Green while the effect is playing
  while (digitalRead(SFX_ACT) == LOW) {
    colorChase(50, 0x00FF00 , BRIGHTNESS);
  }
}

// DARK ZONE
// Entering Dark Zone
void dark_zone_pulse_first_run() {
  char sfx_trackname[20];
  int8_t sfx_count;

  // if we're playing a different sound, we pre-empt it.
  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  // Randomly pick one of the first strings
  sfx_count = random(2);
  switch (sfx_count) {
    case 0:
      String("DZENT00 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 1:
      String("DZENT01 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
  }
  sfx.playTrack(sfx_trackname);

  // Amber Sparks
  while (digitalRead(SFX_ACT) == LOW) {
    //randomSpark (30, 0x994C00, BRIGHTNESS, 32);
    //randomSpark (30, 0xFFC300, BRIGHTNESS, 32);
    //randomSpark (30, 0xFF9933, BRIGHTNESS, 32);
    randomSpark (30, 0xFF6600, BRIGHTNESS, 32);
  }

  // Randomly pick one of the second strings
  String("DZENT10 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
  sfx.playTrack(sfx_trackname);

  // Wait till we're done playing the previous section
  while (digitalRead(SFX_ACT) == LOW);

  // Randomly pick one of the third strings
  sfx_count = random(3);
  switch (sfx_count) {
    case 0:
      String("DZENT20 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 1:
      String("DZENT21 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 2:
      String("DZENT21 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
  }
  sfx.playTrack(sfx_trackname);

  // Purple Sparks
  while (digitalRead(SFX_ACT) == LOW) {
    randomSpark (20, 0xFF00FF, BRIGHTNESS, 32);
  }

  // Random wipe to Purple
  randomFill (50, 0xFF00FF, BRIGHTNESS);
  //color_transition(7, 0xFF00FF);
}

// DZ throbber
void dark_zone_pulse() {
  pulse(7, 0xFF00FF, 20, 180);
}

// play the "Pulse" sound
void fire_a_pulse() {
  char sfx_trackname[20] = "DZPULSE0OGG\n";
  //    String("RAGKIA00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
  sfx.playTrack(sfx_trackname);

  // Color pop Orange
  colorPop(0x994400, 75, 7);
  //  colorPop(0xFF9900, 75, 7);
}

// Kill a Rogue
void kill_a_rogue () {
  char sfx_trackname[20];
  int8_t sfx_count = random(3);

  switch (sfx_count) {
    case 0:
      String("RAGKIA00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 1:
      String("RAGKIA01OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 2:
      String("RAGKIA02OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
  }

  // if we're already playing a track, stop the soundboard first
  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  // Color pop Red
  colorPop(0xFF0000, 100, 5);

  sfx_count = random(5);
  if (sfx_count > 3) {
    while (digitalRead(SFX_ACT) == LOW);
    String("DZRUPSNDOGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
    sfx.playTrack(sfx_trackname);
    colorPop(0xFF00FF, 100, 5);
  }
}

// Kill a Rogue
void exit_DZ_while_rogue () {
  char sfx_trackname[20];
  int8_t sfx_count = random(3);

  switch (sfx_count) {
    case 0:
      String("DZEXTR00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 1:
      String("DZEXTR01OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 2:
      String("DZEXTR02OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
  }

  // if we're already playing a track, stop the soundboard first
  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);
}


// Gone Rogue pulse First Run
void gone_rogue_pulse_first_run() {
  // Transition the spinner to RED
  char sfx_trackname[20];
  int8_t sfx_count;

  // if we have a team, then they can kill things too
  if (team_count) {
    sfx_count = random(6);
  } else {
    sfx_count = random(3);
  }
  switch (sfx_count) {
    case 0:
      String("DZGORI00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 1:
      String("DZGORI01OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 2:
      String("DZGORI02OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 3:
      String("DZGORT00OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 4:
      String("DZGORT01OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
    case 5:
      String("DZGOR02OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      break;
  }

  // if we're already playing a track, stop the soundboard first
  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  if (first_rogue_state) {
    first_rogue_state = false;
    // Red Sparks
    while (digitalRead(SFX_ACT) == LOW) {
      randomSpark (20, 0xFF0000, BRIGHTNESS, 32);
    }

    // Random wipe to Red
    randomFill (50, 0xFF0000, BRIGHTNESS);
    //color_transition(7, 0xFF00FF);
  } else {

    // Color pop Red
    colorPop(0xFF0000, 75, 5);
  }

}

// Gone Rogue pulse
void gone_rogue_pulse() {
  //pulse(7, 0xFF0000, 20, 180);
  colorChase (20, 0xFF0000, BRIGHTNESS);
}

// End Rogue Status
void end_rogue_status() {
  char sfx_trackname[20];
  String("ENDROGUEOGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
  sfx.playTrack(sfx_trackname);

  // Transition back to the regular DZ spinner
  // Red Sparks
  while (digitalRead(SFX_ACT) == LOW) {
    randomSpark (20, 0xFF0000, BRIGHTNESS, 32);
  }
  // Purple Sparks
  while (digitalRead(SFX_ACT) == LOW) {
    randomSpark (20, 0xFF00FF, BRIGHTNESS, 32);
  }

  randomFill (50, 0xFF00FF, BRIGHTNESS);
  //color_transition(7, 0xFF00FF);
}

// Leaving Dark Zone
void dark_zone_pulse_last_run() {
  char sfx_trackname[20];
  String("DZEXT01 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
  sfx.playTrack(sfx_trackname);

  // Purple Sparks
  while (digitalRead(SFX_ACT) == LOW) {
    randomSpark (20, 0xFF00FF, BRIGHTNESS, 32);
  }

  // Amber Sparks
  while (digitalRead(SFX_ACT) == LOW) {
    //randomSpark (30, 0x994C00, BRIGHTNESS, 32);    
    //randomSpark (30, 0xFFC300, BRIGHTNESS, 32);
    //randomSpark (30, 0xFF9933, BRIGHTNESS, 32);
    randomSpark (30, 0xFF6600, BRIGHTNESS, 32);

  }
  //randomFill (50, 0x994C00, BRIGHTNESS);
  //randomFill (50, 0xFFC300, BRIGHTNESS);
  //randomFill (50, 0xFF9933, BRIGHTNESS);
  randomFill (50, 0xFF6600, BRIGHTNESS);
  //color_transition(7, 0xFF00FF);
}

// FUN ZONE
void fun_zone_first_run() {
  char sfx_trackname[20] = "FUNZENT0WAV\n";

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);
  //  sfx.playTrack("FUNZENT1WAV");

  // Blue Sparks
  while (digitalRead(SFX_ACT) == LOW) {
    randomSpark (50, 0x0000FF, BRIGHTNESS, 32);
  }

  color_transition(7, 0x0000FF);
}

void fun_zone_pulse() {
  if (!disco_mode) {
    pulse(7, 0x0000FF, 20, 180);
  } else {
    // See how long we've been running this effect
    // if we're running colorChase, let it run for a bit.
    if (((current_effect == 0 or current_effect == 5)) and current_effect_count < 30) {
      colorChase2 (50, 0, 0, 0, BRIGHTNESS);

    // if we're running randomSpark, let that run for a bit too.
    } else if (current_effect == 5 and current_effect_count < 15) { 
      randomSpark (80, 0, BRIGHTNESS, 20);

    // else, pull a new effect from the hopper and run it
    } else {

      int effect = random(6);
      current_effect = effect;
      current_effect_count=0;

      switch (effect) {
        case 0: // Random Chase
        case 5:
          colorChase2 (50, 0, 0, 0, BRIGHTNESS);
          break;
        case 1: // Random Ladder
          chaseUpDown (70, 0, 0, 1);
          break;
        case 2: // Random Up
          chaseUp (70, 0, 0, 3);
          break;
       case 3: // Random Down
          chaseDown (70, 0, 0, 3);
          break;
        case 4: // Random Stars      
         randomSpark (80, 0, BRIGHTNESS, 20);
         break;
      }
    }

    current_effect_count+=1;
    
  }
}

void toggle_disco_mode() {
  char sfx_trackname[20];
  String("ALERTUP1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  // Toggle Disco Mode on/off
  disco_mode = !disco_mode;

  /*  if (disco_mode) {
      while (digitalRead(SFX_ACT) == LOW);

      String("FUNZREPTWAV\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      sfx.playTrack(sfx_trackname);
    }*/
}

void toggle_fz_standard_throbber() {
  char sfx_trackname[20];
  String("ALERTUP1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  // Toggle FZ Standard Throbber on/off
  fz_standard_throbber = !fz_standard_throbber;

  /*  if (disco_mode) {
      while (digitalRead(SFX_ACT) == LOW);

      String("FUNZREPTOGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
      sfx.playTrack(sfx_trackname);
    }*/
}

void single_track_first_run() {
  char sfx_trackname[20];
  String("ALERTUP1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);
}

void single_track_pulse() {
  // If we're in repeat mode and nothing is playing, restart it.
  if (repeat_play and (digitalRead(SFX_ACT) != LOW)) {
    single_track_stop_play();
  }


  // If we're running w/the fz_standard Pulse do the basic pulse instead.
  if (fz_standard_throbber) {
    state_pulse();
    return;
  }

  switch ( current_track ) {
    case 0:
      colorChase2 (50, 0x660000, 0x666666, 0x000066, BRIGHTNESS);
      break;
    case 1:
      randomSpark (50, 0x0, BRIGHTNESS, 16);
      break;
    case 2:
    case 3:
      colorChase2 (50, 0x00ffff, 0xffff00, 0xff00ff, BRIGHTNESS);
      break;
    case 4:
      //colorChase2(75, 0, 0, 0, BRIGHTNESS);
      chaseUpDown (75, 0xff0000, 0, 2);
      break;
    case 5:
      chaseUpDown (75, 0, 0xff0000, 2);
      break;
    case 6:
      colorChase2 (60, 0xff0000, 0, 0xff99ff, BRIGHTNESS);
      break;
    case 7:
      colorChase2 (80, 0xFFFF00, 0, 0, BRIGHTNESS);
      break;
    case 8:
      //colorChase2 (55, 0xCC9900, 0xff9900, 0xffcc00, BRIGHTNESS);
      colorChase2 (55, 0x80FFFF, 0xff9900, 0xffcc00, BRIGHTNESS);
      break;
    case 9:
      colorChase2 (75, 0x0033cc, 0x66ff33, 0, BRIGHTNESS);
      break;
    case 10:
      randomSpark (80, 0x0, BRIGHTNESS, 10);
      break;
    case 11:
      randomSpark (120, 0xFFFF00, BRIGHTNESS, 8);
      break;
    case 12:
      pulse (7, 0x80FFFF, 20, 180);
      break;
    case 13:
      pulse (7, 0xFFFF00, 20, 180);
      break;
  }
}

void single_track_stop_play() {
  char sfx_trackname[20];

  // if we're playing something, stop it.
  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  } else {
    switch (current_track) {
      case 0:
        String("FUNZT00 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 1:
        String("FUNZT01 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 2:
        String("FUNZT02 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 3:
        String("FUNZT03 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 4:
        String("FUNZT04 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 5:
        String("FUNZT05 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 6:
        String("FUNZT06 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 7:
        String("FUNZT07 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 8:
        String("FUNZT08 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 9:
        String("FUNZT09 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 10:
        String("FUNZT10 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 11:
        String("FUNZT11 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 12:
        String("FUNZT12 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
      case 13:
        String("FUNZT13 OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
        break;
    }
    sfx.playTrack(sfx_trackname);
  }

}

void single_track_next() {
  char sfx_trackname[20];
  String("ALERTUP1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  if (current_track < total_tracks - 1) {
    current_track += 1;
  } else {
    current_track = 0;
  }
}

void single_track_prev() {
  char sfx_trackname[20];
  String("ALERTDN1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  if (current_track > 0) {
    current_track -= 1;
  } else {
    current_track = total_tracks - 1;
  }

}

void single_track_repeat() {
  char sfx_trackname[20];
  String("ALERTUP1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  // Toggle Repeat Play on/off
  repeat_play = !repeat_play;

  if (repeat_play) {
    while (digitalRead(SFX_ACT) == LOW);

    String("FUNZREPTWAV\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
    sfx.playTrack(sfx_trackname);
  }
}

void playlist_first_run() {
  char sfx_trackname[20];
  String("ALERTUP1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  while (digitalRead(SFX_ACT) == LOW);
  sfx.playTrack(sfx_trackname);
}

void playlist_pulse() {
  pulse(7, 0x000066, 20, 180);
}

void playlist_random() {
  char sfx_trackname[20];
  String("ALERTUP1OGG\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));

  if (digitalRead(SFX_ACT) == LOW) {
    sfx.stop();
  }

  sfx.playTrack(sfx_trackname);

  // Toggle Random Play on/off
  random_play = !random_play;
  if (random_play) {
    while (digitalRead(SFX_ACT) == LOW);

    String("FUNZRANDWAV\n").toCharArray(sfx_trackname, sizeof(sfx_trackname));
    sfx.playTrack(sfx_trackname);
  }
}

// SETTINGS ZONE
void settings_zone_first_run() {
  char sfx_trackname[20] = "SETZONE0WAV\n";
  sfx.playTrack(sfx_trackname);
  //  sfx.playTrack("SETZONE1WAV");

  // White Sparks
  while (digitalRead(SFX_ACT) == LOW) {
    randomSpark (50, 0x999999, BRIGHTNESS, 16);
  }

  color_transition(7, 0x999999);
}

void settings_zone_pulse() {
  pulse(7, 0x999999, 20, 180);
}
/*
  SerialEvent occurs whenever a new data comes in the
  hardware serial RX.  This routine is run between each
  time loop() runs, so using delay inside loop can delay
  response.  Multiple bytes of data may be available.
*/
void serialEvent1() {
  while (Serial1.available()) {
    // get the new byte:
    char inChar = (char)Serial1.read();
    // add it to the input_string:
    input_string += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      string_complete = true;
    }
  }
}


void pulse(uint8_t wait, uint32_t color, uint32_t throb_min, uint32_t throb_max) {
  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, color); // Set pixels on
  }
  if (throb_brightness > (throb_max + (throb_max - throb_min))) {
    throb_brightness = throb_min;
  } else if (throb_brightness > throb_max) {
    ring.setBrightness(throb_max - (throb_brightness - throb_max));
  } else {
    ring.setBrightness(throb_brightness);
  }
  ring.show();
  throb_brightness++;
  delay(wait); // Delay in milliseconds
}

// Color chase up and down both sides of the LED
// implemented as calls to the underlying Up and Down subroutines
void chaseUpDown( uint8_t wait, uint32_t color1, uint32_t color2, uint16_t cycles) {
  for (uint16_t i = 0; i < cycles; i++) {
    chaseUp( wait, color1, color2, 1);
    chaseDown( wait, color1, color2, 1);
  }
}

// Color chase up both sides of the LED
void chaseUp(uint8_t wait, uint32_t color1, uint32_t color2, uint16_t cycles) {

  // if color1 == color2 == 0 then use random colors instead.
  bool random_mode = false;

  if ((color1 == color2) && (color2 == 0)) {
    random_mode = true;
  }

  // Clear pixels to color1
  for (uint16_t i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, color1);
    ring.show();
  }

  // Cycles color chase on color2
  for (uint16_t i = 0; i < cycles; i++) {

    //if (random_mode) {
    //  color2 = ring.Color(random(1, 251), random(1, 251), random(1, 251));
    //}

    // Chase up
    for (uint16_t i = 0; i < ring.numPixels() / 2; i++) {
      // set old pixels to old color
      if (i > 0) {
        ring.setPixelColor(i - 1, color1);
        ring.setPixelColor(ring.numPixels() - i + 1, color1);
        ring.show();
        //delay(wait);
      }

      if (random_mode) {
        color2 = ring.Color(random(1, 251), random(1, 251), random(1, 251));
      }

      // set new pixels to new color
      ring.setPixelColor(i, color2);
      ring.setPixelColor(ring.numPixels() - i, color2);
      ring.show();
      delay(wait);
    }

  }
}

// Color chase down both sides of the LED
void chaseDown(uint8_t wait, uint32_t color1, uint32_t color2, uint16_t cycles) {
  // if color1 == color2 == 0 then use random colors instead.
  bool random_mode = false;

  if ((color1 == color2) && (color2 == 0)) {
    random_mode = true;
  }

  // Clear pixels to color1
  for (uint16_t i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, color1);
    ring.show();
  }

  // Cycles color chase on color2
  for (uint16_t i = 0; i < cycles; i++) {

    // Chase down
    for (uint16_t i = ring.numPixels() / 2; i > 0; i--) {
      if (i < (ring.numPixels() / 2)) {
        ring.setPixelColor(i + 1, color1);
        ring.setPixelColor(ring.numPixels() - i - 1, color1);
        ring.show();
        //delay(wait);
      }

      if (random_mode) {
        color2 = ring.Color(random(1, 251), random(1, 251), random(1, 251));
      }

      // set new pixels to new color
      ring.setPixelColor(i, color2);
      ring.setPixelColor(ring.numPixels() - i, color2);
      ring.show();
      delay(wait);
    }

  }
}


/* need to fold colorChase into colorChase2 and come up with counter clockwise versions of them */
void colorChase (uint8_t wait, uint32_t color, uint32_t brightness) {
  ring.setBrightness(brightness); // Set to standard brightness
  for (uint8_t pixel = 0; pixel < ring.numPixels(); pixel++) {
    uint32_t c = 0;
    if (((offset + pixel) & 7) < 2) c = color; // 4 pixels on...
    ring.setPixelColor(pixel, c); // Set pixel
  }
  ring.show();
  offset++;
  delay(wait);
}

void colorChase2 (uint8_t wait, uint32_t color1, uint32_t color2, uint32_t color3, uint32_t brightness) {
  bool random_mode = false;
  if ((color1 == color2) and (color1 == color3) and (color1 == 0)) {
    random_mode = true;
  }
  
  ring.setBrightness(brightness); // Set to standard brightness
  for (uint8_t pixel = 0; pixel < ring.numPixels(); pixel++) {
    uint32_t c;
    if (((offset + pixel) & 7) < 2) {
      //c = color1; // 4 pixels on...
      if (random_mode) {
        color1 = ring.Color(random(1, 251), random(1, 251), random(1, 251));
        color2 = ring.Color(random(1, 251), random(1, 251), random(1, 251));
        color3 = ring.Color(random(1, 251), random(1, 251), random(1, 251));
      }
      ring.setPixelColor(pixel, color1); // Set pixel
      ring.setPixelColor(pixel - 1, color2);
      ring.setPixelColor(pixel - 2, color3);
    } else {
      ring.setPixelColor(pixel, 0); // Set pixel
    }
  }
  ring.show();
  offset++;
  delay(wait);
}

// Effect for "broken pixels all over the screen (random "breaking up"/"starting up"?)
// If color is 0 (black) then we assume random colors instead.
// (might switch to a two color call version so it has more utility?)
void randomSpark (uint8_t wait, uint32_t color, uint32_t brightness, uint32_t count) {
  uint8_t pixel;

  // clear ring
  for (uint8_t i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(pixel, 0);
  }
  ring.show();

  // reset brightness to "normal" (normalizes from pulse)
  ring.setBrightness(brightness); // Set to standard brightness

  for (uint8_t i = 0; i < count; i++) {
    // get the LED count from the ring and get a random pixel
    pixel = random(ring.numPixels());

    if (!color) {
      ring.setPixelColor( pixel, ring.Color(random(1, 251), random(1, 251), random(1, 251)) );
    } else {
      ring.setPixelColor( pixel, color );
    }
    
    ring.show();
    delay(wait);
    ring.setPixelColor(pixel, 0);
  }
}

// Effect to fill in the ring/strip in a random pattern
void randomFill (uint8_t wait, uint32_t color, uint32_t brightness) {
  uint8_t pixel;
  uint8_t temp_pixel;
  int8_t rand_pixels[ring.numPixels()];

  // set up an array of the pixel indexes
  for (uint8_t i = 0; i < ring.numPixels(); i++) {
    rand_pixels[i] = i;
  }

  // shuffle them into a random order
  for (uint8_t i = 0; i < ring.numPixels(); i++) {
    // get the LED count from the ring and get a random pixel
    pixel = random(ring.numPixels());
    temp_pixel = rand_pixels[i];
    rand_pixels[i] = rand_pixels[pixel];
    rand_pixels[pixel] = temp_pixel;
  }

  // fill those random indexes
  for (uint8_t i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(rand_pixels[i], color);
    ring.show();
    delay(wait);
    //    ring.setPixelColor(pixel, 0);
  }

}


// Effect to Fill in randomly all over (like broken but filling in vs. specks)

// Transition effect from current color, down to black and then back up to new color
// *** Currently broken, this just performs a staggerd wipe to color ***
void color_transition(uint8_t wait, uint32_t color) {
  /*
    // bring brightness down to zero
    for (uint16_t i=throb_brightness; i>0; i--) {
      ring.setBrightness(i);
      delay(wait);
      ring.show();
    }
    // change color
    for(uint16_t i=0; i<ring.numPixels(); i++) {
      ring.setPixelColor(i, color);
    }

    // bring brightness up to current level
    for (uint16_t i=throb_brightness; i>0; i--) {
      ring.setBrightness(i);
      delay(wait);
      ring.show();
    }

  */
  /* Temp fix, switch to color wiping transition */
  colorWipe(color, wait * 300);
  delay(wait);

}


// Fill the dots one after the other with a color
void colorWipe(uint32_t color, uint8_t wait) {
  for (uint16_t i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, color);
    ring.show();
    delay(wait);
  }
}


// "pop" the color to black X number of times w/delays
void colorPop (uint32_t color, uint16_t wait, uint16_t times) {
  // go to black
  for (uint16_t i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, 0);
  }
  ring.show();

  for (uint16_t i = 0; i < times; i++) {
    // pop color
    for (uint16_t i = 0; i < ring.numPixels(); i++) {
      ring.setPixelColor(i, color);
    }
    ring.show();
    delay(wait);

    // go to black
    for (uint16_t i = 0; i < ring.numPixels(); i++) {
      ring.setPixelColor(i, 0);
    }
    ring.show();

    // delay
    delay(wait * 2);
  }
}

