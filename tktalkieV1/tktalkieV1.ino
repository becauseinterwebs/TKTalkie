/****
 * TK TALKIE by Brent Williams <becauseinterwebs@gmail.com>
 * 
 * Version 1.0
 * 
 * WhiteArmor.net User ID: lerxstrulz
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
 * The audio components and connections were made using the GUI tool 
 * available at http://www.pjrc.com/teensy/gui.  If you want to modify 
 * the signal path, add more effects, etc., you can copy and paste the 
 * code marked by 'GUITool' into the online editor at the above URL.
 * 
 * A lot of this code was taken from the example code that comes with 
 * Teensy/Arduino and modified.
 * 
 * www.BecauseInterwebs.com
 * 
 */
 
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// You should be able to import the below code block into the GUI editor at 
// http://www.pjrc.com/teensy/gui if you want to add any components or change 
// any connections.

// GUItool: begin automatically generated code
AudioPlaySdWav           playSdWav1;     //xy=111,219
AudioInputI2S            i2s1;           //xy=122,100
AudioEffectBitcrusher    bitcrusher1;    //xy=407,61
AudioEffectBitcrusher    bitcrusher2;    //xy=419,118
AudioAnalyzeRMS          rms1;           //xy=427,239
AudioMixer4              mixer1;         //xy=699,113
AudioOutputI2S           i2s2;           //xy=955,109
AudioConnection          patchCord1(playSdWav1, 0, mixer1, 2);
AudioConnection          patchCord2(playSdWav1, 1, mixer1, 3);
AudioConnection          patchCord3(i2s1, 0, bitcrusher1, 0);
AudioConnection          patchCord4(i2s1, 1, bitcrusher2, 0);
AudioConnection          patchCord5(i2s1, 1, rms1, 0);
AudioConnection          patchCord6(bitcrusher1, 0, mixer1, 0);
AudioConnection          patchCord7(bitcrusher2, 0, mixer1, 1);
AudioConnection          patchCord8(mixer1, 0, i2s2, 0);
AudioConnection          patchCord9(mixer1, 0, i2s2, 1);
AudioControlSGTL5000     audioShield;                           
// GUItool: end automatically generated code

// These pins are for the Teensy Audio Adaptor SD Card reader
const int SDCARD_CS_PIN    = 10;
const int SDCARD_MOSI_PIN  = 7;
const int SDCARD_SCK_PIN   = 14;

const int MIN_SILENCE_TIME        = 350;    // The minimum time to wait before playing a sound effect after talking
const float VOL_THRESHOLD_TRIGGER = 0.18;   // The amplitude needed to trigger the sound effects process
const float VOL_THRESHOLD_MIN     = 0.03;   // The minimum amplitude to use when determining if you've stopped talking.
                                            // Depending upon the microphone you are using, you may get a constant signal
                                            // that is above 0 or even 0.01.  Use the Serial monitor and add output to the 
                                            // loop to see what signal amplitude you are receiving when the mic is "quiet."

elapsedMillis ms;                           // running timer...inputs are checked every 24 milliseconds
elapsedMillis stopped;                      // used to tell how long user has stopped talking
int speaking = 0;                           // flag to let us know if the user is speaking or not

String wavFiles[99];                        // This will hold an array of the WAV files on the SD card.
                                            // 99 is an arbitrary number.  You can change it as you need to.
int wavCount = 0;                           // This keeps count of how many valid WAV files were found.
int lastRnd  = 0;                           // Keeps track of the last file played so that it is different each time

/***
 * Read the contents of the SD card and put any files staring with "TKT_" and ending 
 * with ".WAV" into the array.  It will recursively search directories.  
 */
void loadFiles(File dir) {
   
   while(true) {
     
     File entry =  dir.openNextFile();
     
     if (! entry) {
       // no more files
       break;
     }
     
     if (entry.isDirectory()) {
       
       // If you DON'T want to recursively search directories, 
       // comment out this line.
       loadFiles(entry);
       
     } else {
       // add to file list if it starts with "TKT_" and ends with ".WAV"
      
       // convert to string to make it easier to work with...
       String fname = (String)entry.name();

       // Ignore it if there is a "~" because that usually means it's a backup
       if (fname.indexOf(".WAV") > 0 && fname.indexOf("TKT_") == 0 && fname.indexOf("~") < 0) {
         wavFiles[wavCount] = entry.name();
         wavCount++;
       }
       
     }
     
     entry.close();
     
   }
   
}

/***
 * This plays the specified WAV file from the SD Card
 */
void playFile(const char* filename)
{

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playSdWav1.play(filename);

  // Simply wait for the file to finish playing.
  while (playSdWav1.isPlaying()) {
    // You could check inputs here if you wanted...
  }

}

/***
 * Play a random sound effect from the SD card
 */
void playSoundEffect()
{

  if (speaking == 1) return;
  
  // generate a random number between 0 and the number of files read - 1
  int rnd = lastRnd;
  while (rnd == lastRnd) {
    rnd = random(0, wavCount);
  }
  lastRnd = rnd;

  // file names are stored as strings in the array
  String filename = wavFiles[rnd];
  
  // have to convert the String back to a null-terminated char array for the SD card WAV player...
  char buf[filename.length()+2];
  filename.toCharArray(buf, filename.length()+2);

  // play the file
  playFile(buf);
}

/**
 * This is a simple function that allows the program
 * to keep running while we wait for something...
 */
void wait(unsigned int milliseconds)
{
  elapsedMillis msec=0;
  while (msec <= milliseconds) {
    // If you wanted to check inputs from buttons or potentiometers or whatever
    // you could do it here...
  }
}

/***
 * Check the optional volume pot for output level
 */
void readVolume()
{
    // comment these lines if your audio shield does not have the optional volume pot soldered on
    float vol = analogRead(15);
    vol = vol / 1024;
    audioShield.volume(vol);
}

/***
 * Initial setup...runs only once when the board is turned on.
 */
void setup() 
{
  
  // You really only need the Serial connection 
  // for output while you are developing, so you 
  // can uncomment this and use Serial.println()
  // to write messages to the console.
  //Serial.begin(9600);
  
  // Always allocate memory for the audio shield!
  AudioMemory(64);
  
  // turn on audio shield
  audioShield.enable();
  
  // volume level is 0.0 to 1.0
  readVolume();
  
  // tell the audio shield to use the MIC pins
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  
  // uncomment to use the LINE-IN pins instead...
  //audioShield.inputSelect(AUDIO_INPUT_LINEIN);
  
  // Activate the onboard pre-processor
  audioShield.audioPreProcessorEnable();
  // Turn on the 5-band graphic equalizer (there is also a 7-band parametric...see the Teensy docs)
  audioShield.eqSelect(3);
  // Bands (from left to right) are: Low, Low-Mid, Mid, High-Mid, High.
  // Valid values are -1 (-11.75dB) to 1 (+12dB)
  // The settings below pull down the lows and highs and push up the mids for 
  // more of a "tin-can" sound.
  audioShield.eqBands(-1.0, 0, 1, 0, -1.0);
  
  // adjust the gain of the input
  // adjust this as needed
  audioShield.micGain(36);

  // You can modify these values to process the voice 
  // input.  See the Teensy bitcrusher demo for details.
  bitcrusher1.bits(12);
  bitcrusher1.sampleRate(16384);
  
  bitcrusher2.bits(10);
  bitcrusher2.sampleRate(10240);
  
  // stereo input channels...just a little gain 
  // for the MIC or LINE-IN inputs...adjust as needed.
  mixer1.gain(0, 0.2);
  mixer1.gain(1, 0.2);
  
  // stereo channels for SD card...adjust gain as 
  // necessary to match voice level
  mixer1.gain(2, 0.9);
  mixer1.gain(3, 0.9);
  
  // Check SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    
    // Serial.println("Unable to access the SD card");
    
  } else {
    
    // Read the contents of the SD card
    File root;
    root = SD.open("/");
    loadFiles(root);
    root.close();
    
    // Just for fun...play a startup WAV file if it exists ;)
    playFile("STARTUP.WAV");
    
  }

  // this just makes sure we get truly random numbers each time
  // when choosing a file to play from the list later on...
  randomSeed(analogRead(0));

}

/***
 * Main program loop
 */
void loop() 
{
  
  // Check every 24 milliseconds to see what's going on...
  if (ms > 24) {
    
    // reset the counter!
    ms = 0;
    
    // Check if we have audio
    if (rms1.available()) {
      
      // get the input amplitude...will be between 0.0 (no sound) and 1.0
      float val = rms1.read();

      // Uncomment this to see what your constant signal is
      //Serial.println(val);
      
      // Set a minimum and maximum level to use or you will trigger it accidentally 
      // by breathing, etc.  Adjust this level as necessary!
      if (val >= VOL_THRESHOLD_TRIGGER) {
         
        // If we are getting a signal, reset the "user is talking" timer
        stopped = 0;
        
        // If user is not currently speaking, then they just started talking :)
        if (speaking == 0) {

          // Set the flag letting us know the user is speaking
          speaking = 1;
          
        }
        
      } else if (speaking == 1) {
        
          if (val < VOL_THRESHOLD_MIN) {

            // If the user has stopped talking for at least the required "silence" time and 
            // the mic/line input has fallen below the minimum input threshold, play a random 
            // sound from the card.  NOTE:  You can adjust the delay time to your liking...
            // but it should probably be AT LEAST 1/4 second (250 milliseconds.)

            if (stopped >= MIN_SILENCE_TIME) {
              
              // reset the speaking flag
              speaking = 0;
    
              // play random sound effect
              playSoundEffect();

            }

          } else {
              
              // Reset the "silence" counter 
              stopped = 0;
              
          }

      }
      
    }

    readVolume();
    
  }
  
}

// END
