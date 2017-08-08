// Test code to make sure everything in the transmitter is wired together
// The goal is to allow the operator (me)
// to test the code, both visually, and through the single operator light.
//
// If I like this code, it may survive to the final product.
// (update 1 year later ... it did :) )
//
// Hardware:
// XBee S1
// always high 1,2,17

const int LEDPin = 3;

const int BlackButtonPin = 19;
const int BlueButtonPin = 20;
const int GreenButtonPin = 21;
const int RedButtonPin = 22;
const int WhiteButtonPin = 23;

int BlackButton = 0;
int BlueButton = 0;
int GreenButton = 0;
int RedButton = 0;
int WhiteButton = 0;

int SendButton = 0;

void setup() {
  // put your setup code here, to run once:

  // Self monitoring pins
  pinMode(11, INPUT);
  pinMode(13, INPUT); // Battery monitoring
  
  // Set Output pins
  pinMode(LEDPin, OUTPUT);

  // Set Input pins from buttons
  pinMode(BlackButtonPin,INPUT);
  pinMode(BlueButtonPin,INPUT);
  pinMode(GreenButtonPin,INPUT);
  pinMode(RedButtonPin,INPUT);
  pinMode(WhiteButtonPin,INPUT);

  // Pop pullups because we are using two terminal Buttons
  digitalWrite(BlackButtonPin, HIGH);
  digitalWrite(BlueButtonPin, HIGH);
  digitalWrite(GreenButtonPin, HIGH);
  digitalWrite(RedButtonPin, HIGH);
  digitalWrite(WhiteButtonPin, HIGH);

  // Open a connection for debugging
  Serial.begin(9600);

  // Open a connection to the XBee
  Serial1.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  //digitalWrite(3, HIGH);
  analogWrite(3, 50);
//  delay(1000);              // wait for a second
//  digitalWrite(3, LOW);    // turn the LED off by making the voltage LOW
//  delay(1000);

  // If a button has been pressed, track it
  if ((digitalRead(BlackButtonPin) == LOW) and (BlackButton == 0)) {
    BlackButton = 1;
    SendButton = 1;
  }
  if ((digitalRead(BlueButtonPin) == LOW) and (BlueButton == 0)) {
    BlueButton = 1;
    SendButton = 2;
  }
  if ((digitalRead(GreenButtonPin) == LOW) and (GreenButton == 0)) {
    GreenButton = 1;
    SendButton = 3;
  }
  if ((digitalRead(RedButtonPin) == LOW) and (RedButton == 0)) {
    RedButton = 1;
    SendButton = 4;
  }
  if ((digitalRead(WhiteButtonPin) == LOW) and (WhiteButton == 0)) {
    WhiteButton = 1;
    SendButton = 5;
  }

  // Send Button Press and reset button
  if (SendButton != 0) {
    // Send the button press over the XBee to the receiver
    Serial.println(SendButton);
    Serial1.println(SendButton);
    
    // Cycle the indicator LED to confirm the button press
    //digitalWrite(3, LOW);
    analogWrite(3, 0);
//    delay(1000);
    for (int i = 0; i < SendButton; i++) {
      // digitalWrite(3, HIGH);
      analogWrite(3, 126);
      delay(100);
      // digitalWrite(3, LOW);
      analogWrite(3, 0);
      delay(150);      
    }

    // add extra artificial delay for the low buttons so we don't accidentaly trigger them twice
    switch (SendButton) {
      case 1:
        delay(250);
      case 2:
        delay(250);
      case 3:
        delay(250);
      case 4:
        delay(250);
      case 5:
        break;      
    }
    delay(250);
    

 //   delay(1000);
//    digitalWrite(3,HIGH);
    analogWrite(3, 50);
  }

  // Reset Buttons
  BlackButton = 0;
  BlueButton = 0;
  GreenButton = 0;
  RedButton = 0;
  WhiteButton = 0;
  SendButton = 0;
}
