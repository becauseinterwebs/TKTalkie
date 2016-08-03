/****
 * TK TALKIE by Brent Williams <becauseinterwebs@gmail.com>
 * 
 * This simple sketch is meant to use a Teensy 3.2 with a 
 * Teensy Audio Shield and reads sounds from an SD card and plays 
 * them after the user stops talking to simulate comm noise 
 * such as clicks and static.  It can be easily adapted to be 
 * used with other controllers and audio shields.
 * 
 * This sketch incorporates some of the sample code provided with 
 * Teensy and Arduino.
 * 
 * You are free to use this code in your own projects, provided 
 * they are non-commercial in use.
 * 
 * www.BecauseInterwebs.com
 * 
 */
 
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioPlaySdWav           playSdWav1;                            // SD WAV file player   
AudioInputI2S            i2s1;                                  // Stereo input (wired to MIC or LINE-IN pins)
AudioOutputI2S           i2s2;                                  // Stereo ouput built-in to audio shield
AudioAnalyzePeak         peak1;                                 // Used to determine when user is speaking or not
AudioMixer4              mixer1;                                // Used control gains and route signals

AudioConnection          patchCord1(playSdWav1, 0, mixer1, 2);  // Connect SD WAV player to mixer (left)
AudioConnection          patchCord2(playSdWav1, 1, mixer1, 3);  // Connect SD WAV player to mixer (right)
AudioConnection          patchCord3(i2s1, 0, mixer1, 0);        // Connect input to mixer (left)
AudioConnection          patchCord4(i2s1, 1, mixer1, 1);        // Connect input to mixer (right)
AudioConnection          patchCord5(i2s1, 1, peak1, 0);         // Connect input to peak monitor 
AudioConnection          patchCord6(mixer1, 0, i2s2, 0);        // Connect mixer to output (left)
AudioConnection          patchCord7(mixer1, 0, i2s2, 1);        // Connect mixer to output (right)
AudioControlSGTL5000     audioShield;                           // This is the Teensy audio shield

const int LED_PIN          = 13;
const int SDCARD_CS_PIN    = 10;
const int SDCARD_MOSI_PIN  = 7;
const int SDCARD_SCK_PIN   = 14;
const int MIN_SILENCE_TIME = 250;

elapsedMillis ms;         // running timer...inputs are checked every 30 milliseconds
elapsedMillis stopped;    // used to tell how long user has stopped talking
int speaking = 0;         // flag to let us know if the user is speaking or not

char* wavFiles[] = {};    // This will hold an array of the WAV files on the SD card

/***
 * Initial setup...runs only once when the board is turned on.
 */
void setup() 
{
  // You really only need the Serial connection 
  // for output while you are developing, so you 
  // may want to comment this out.
  Serial.begin(9600);
  // Setup on-board LED
  pinMode(LED_PIN, OUTPUT);
  // Always allocate memory for the audio shield!
  AudioMemory(64);
  // turn on audio shield
  audioShield.enable();
  // volume level is 0.0 to 1.0
  audioShield.volume(0.6);
  // tell the audio shield to use the MIC pins
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  // uncomment to use the LINE-IN pins instead...
  //audioShield.inputSelect(AUDIO_INPUT_LINEIN);
  audioShield.micGain(36);
  // stereo input channels...just a little gain 
  // for the MIC or LINE-IN inputs...adjust as needed.
  mixer1.gain(0, 0.2);
  mixer1.gain(1, 0.2);
  // stereo channels for SD card...adjust gain as 
  // necessary to match voice level
  mixer1.gain(2, 0.4);
  mixer1.gain(3, 0.4);
  // Check SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // Play some beeps to let us know that there is 
    // a problem with the SD card, but DON'T stop the
    // program
    log("Unable to access the SD card");
    blink(150, 5);
  } else {
    // Play a startup sound and read the contents 
    // of the SD card
    blink(250, 3);
    File root;
    root = SD.open("/");
    loadFiles(root);
  }
  
  // Wait a second...
  delay(1000);
  
}

/***
 * Main program loop
 */
void loop() 
{
  // Every 30 milliseconds...
  if (ms > 30) {
    // reset!
    ms = 0;
    // This should always be true...
    if (peak1.available()) {
      // get the peak value...will be between 0.0 (no sound) and 1.0
      float val = peak1.read();
      // Set a minimum peak level to use or you will trigger it accidentally 
      // by breathing, etc.  Adjust this level as necessary!
      if (val > 0.10) {
        // If we are getting a signal, reset the "user is talking" timer
        stopped = 0;
        // If user is not currently speaking, then they just started talking :)
        if (speaking == 0) {
          // Set the flag letting us know the user is speaking
          speaking = 1;
          log("SPEAKING");
          log(val);
        }
        
      } else if (speaking == 1 && stopped > MIN_SILENCE_TIME) {
          // if the peak value is 0.0 (no sound) then if the user WAS speaking, let's wait 
          // a bit to see if they are done.  If they are done, play a random sound from the 
          // SD card.  You can adjust the delay time to your liking...but it should probably 
          // be AT LEAST 1/4 second (250 milliseconds.)
          speaking = 0;
          log("PLAY");
          playFile("CLICK1.WAV");
      }
    }
  }
}



/***
 * Read the contents of the SD card and put any files ending 
 * with ".WAV" into the array.  It will recursively search 
 * directories.  
 */
void loadFiles(File dir) {
   
   while(true) {
     
     File entry =  dir.openNextFile();
     
     if (! entry) {
       // no more files
       break;
     }
     
     log(entry.name());
     
     if (entry.isDirectory()) {
       
       // If you DON'T want to recursively search directories, 
       // comment out this line.
       loadFiles(entry);
       
     } else {
       // add to file list if it ends with ".WAV"
      
       // get the number of elements in the array...
       int len = sizeof(wavFiles)/sizeof(char *);
       Serial.println(len);
       
       // add 1 to it...
       len++;
       
       // convert to string to make it easier to work with...
       String fname = (String)entry.name();
       
       // if ".WAV" is in the file name, add it to the list.
       if (fname.indexOf(".WAV") > 0) {
         wavFiles[len] = entry.name();
       }
       
     }
     
     entry.close();
     
   }
   
}

/***
 * This plays the specified WAV file from the SD Card
 */
void playFile(const char *filename)
{
  
  log("Playing file: ");
  log(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playSdWav1.play(filename);

  // A brief delay for the library read WAV info
  delay(5);

  // Simply wait for the file to finish playing.
  while (playSdWav1.isPlaying()) {
    // uncomment these lines if you audio shield
    // has the optional volume pot soldered
    //float vol = analogRead(15);
    //vol = vol / 1024;
    //audioShield.volume(vol);
  }
}

/**
 * Blinks the internal LED
 */
void blink(int wait, int loops)
{
  for (int i = 0; i <= loops ; i++) {
    digitalWrite(LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(wait);                   
    digitalWrite(LED_PIN, LOW);    // turn the LED off by making the voltage LOW
    if (i < loops) {
      // don't wait if it's the last iteration...
      delay(wait); 
    }
  }
}

/**
 * Outputs message to the serial monitor
 */
void log(const char *msg)
{
  // comment this line out if you don't 
  // want output sent to the serial monitor.
  Serial.println(msg);
}

