/****
 * TK TALKIE by Brent Williams <becauseinterwebs@gmail.com>
 * 
 * Version 2.0 (Nov 26, 2016)
 * 
 * WhiteArmor.net User ID: lerxstrulz
 * 
 * This sketch is meant to use a Teensy 3.2 with a 
 * Teensy Audio Shield and reads sounds from an SD card and plays 
 * them after the user stops talking to simulate comm noise 
 * such as clicks and static.  It can be easily adapted to be 
 * used with other controllers and audio shields.
 * 
 * You are free to use this code in your own projects, provided 
 * they are non-commercial in use.
 * o
 * The audio components and connections were made using the GUI tool 
 * available at http://www.pjrc.com/teensy/gui.  If you want to modify 
 * the signal path, add more effects, etc., you can copy and paste the 
 * code marked by 'GUITool' into the online editor at the above URL.
 * 
 * WHAT'S NEW:
 * 
 * V2.2 (Jan 24, 2017)
 *  1.  Added support for using LINE-OUT in addition to headphone
 *  2.  Added new setting lineout=<value> to control line-out voltage
 *  3.  Added new setting for linein=<value> to control line-in level
 *  
 * V2.1 (Dec 16, 2106)
 *  1.  A few bug fixes including saving of master volume and parsing of 
 *      settings file.
 *  2.  Fixed compiler error for unsigned integer check
 *      
 * V2.0 (Nov 26, 2016)
 *  1.  Added support for background loop (chatter) file
 *  2.  Added text config file to hold setting instead of having to modify 
 *      and recompile source code.
 *  3.  Added support for serial interface to allow live editing of settings
 *  4.  Added calibration wizard
 *  
 * V1.1 (Aug 26, 2016)
 *  1.  Added support for PTT (Push-To-Talk) Button.  If a button is present, it 
 *      is activated on first press.  Press it for 2 seconds without talking to 
 *      go back to Voice Activated mode.
 *  2.  Added a pink noise generator to play behind the voice when talking to give 
 *      it more of a "radio" sound.
 *  3.  Added warning beeps to let you know when you are going back into Voice
 *      Activated mode.
 *  4.  Added "voiceOn" and "voiceOff" functions to turn voice inputs on the mixer on     
 *      and off as needed (automatically turns on and off pink noise as well.)  Now 
 *      the Voice Activated mode will go completely silent when not talking and turn back 
 *      on when you start talking!
 *      
 * www.tktalkie.com
 * 
 * 
 */
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce2.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=196,125
AudioPlaySdWav           loopPlayer;     //xy=199,402
AudioSynthWaveform       waveform1;      //xy=202,316
AudioPlaySdWav           effectsPlayer;  //xy=208,266
AudioAnalyzeRMS          rms1;           //xy=325,174
AudioSynthNoisePink      pink1;          //xy=473,176
AudioEffectBitcrusher    bitcrusher1;    //xy=487,90
AudioEffectBitcrusher    bitcrusher2;    //xy=487,133
AudioMixer4              loopMixer;      //xy=656,412
AudioMixer4              voiceMixer;     //xy=659,142
AudioMixer4              effectsMixer;   //xy=660,280
AudioOutputI2S           i2s2;           //xy=836,139
AudioConnection          patchCord1(i2s1, 0, rms1, 0);
AudioConnection          patchCord2(i2s1, 0, bitcrusher1, 0);
AudioConnection          patchCord3(i2s1, 0, bitcrusher2, 0);
AudioConnection          patchCord4(loopPlayer, 0, loopMixer, 0);
AudioConnection          patchCord5(loopPlayer, 1, loopMixer, 1);
AudioConnection          patchCord6(waveform1, 0, effectsMixer, 2);
AudioConnection          patchCord7(effectsPlayer, 0, effectsMixer, 0);
AudioConnection          patchCord8(effectsPlayer, 1, effectsMixer, 1);
AudioConnection          patchCord9(pink1, 0, voiceMixer, 2);
AudioConnection          patchCord10(bitcrusher1, 0, voiceMixer, 0);
AudioConnection          patchCord11(bitcrusher2, 0, voiceMixer, 1);
AudioConnection          patchCord12(loopMixer, 0, effectsMixer, 3);
AudioConnection          patchCord13(voiceMixer, 0, i2s2, 0);
AudioConnection          patchCord14(voiceMixer, 0, i2s2, 1);
AudioConnection          patchCord15(effectsMixer, 0, voiceMixer, 3);
AudioControlSGTL5000     audioShield;    //xy=846,428
// GUItool: end automatically generated code

// These pins are for the Teensy Audio Adaptor SD Card reader - DO NOT CHANGE!
const int SDCARD_CS_PIN    = 10;
const int SDCARD_MOSI_PIN  = 7;
const int SDCARD_SCK_PIN   = 14;

elapsedMillis ms;                           // running timer...inputs are checked every 24 milliseconds
elapsedMillis stopped;                      // used to tell how long user has stopped talking
boolean speaking = false;                   // flag to let us know if the user is speaking or not

String wavFiles[99];                        // This will hold an array of the WAV files on the SD card.
                                            // 99 is an arbitrary number.  You can change it as you need to.
int wavCount = 0;                           // This keeps count of how many valid WAV files were found.
int lastRnd  = 0;                           // Keeps track of the last file played so that it is different each time

Bounce PTT = Bounce();                      // Used to read the PTT button (if attached)

boolean silent = false;                     // used for PTT and to switch back to Voice Activated mode
boolean button_initialized = false;         // flag that lets us know if the PTT has been pushed or not to go into PTT mode

const int EFFECTS_PLAYER = 1;
const int LOOP_PLAYER = 2;

const String SETTINGS_FILE = "TKCONFIG.TXT";
const String BACKUP_FILE   = "SETTINGS.BAK";

int STATE;

const int BOOTING  = 0;
const int BOOTED   = 1;
const int STARTUP  = 2;
const int STARTING = 3;
const int STARTED  = 4;
const int RUNNING  = 5;

// Default settings - can be modified through settings file
float    MASTER_VOLUME; 
int      LINEOUT         = 29; // Valid values 13 to 31. Default teensy setting is 29.
int      LINEIN          = 5;  // Value values 0 to 15. Default teensy setting is 5;
int      HIPASS          = 0; // off by default, 1 = on
int      MIC_GAIN        = 15;
String   STARTUP_WAV     = "STARTUP.WAV";
String   LOOP_WAV        = "";
String   BUTTON_WAV      = "TKT_CLK3.WAV";
int      AUDIO_INPUT     = AUDIO_INPUT_MIC;
int      EQ_TYPE         = FLAT_FREQUENCY;
int      EQ_BANDS_SIZE   = 5;
float    EQ_BANDS[5]     = { -1.0,0,1,0,-1.0 };
int      BITCRUSHER_SIZE = 4;
int      BITCRUSHER[4]   = { 12,16384,10,10240 };
float    LOOP_GAIN       = 4;
float    VOICE_GAIN      = 1;
float    NOISE_GAIN      = 1;
float    BUTTON_GAIN     = 1;
float    EFFECTS_GAIN    = 5;
uint16_t SILENCE_TIME    = 350;    // The minimum time to wait before playing a sound effect after talking has stopped
float    VOICE_START     = 0.07;   // The amplitude needed to trigger the sound effects process
float    VOICE_STOP      = 0.02;   // The minimum amplitude to use when determining if you've stopped talking.
                                   // Depending upon the microphone you are using, you may get a constant signal
                                   // that is above 0 or even 0.01.  Use the Serial monitor and add output to the 
                                   // loop to see what signal amplitude you are receiving when the mic is "quiet."
int      BUTTON_PIN;               // The pin to which a PTT button is connected (not required.) Change it if you 
                                   // attach it to a different pin (should use 0, 1 or 2, though...as not all pins
                                   // are available since they are used by the Audio Adaptor.
boolean DEBUG = false;             // Set to true to have debug messages printed out...useful for testing

elapsedMillis loopMillis = 0;
unsigned int loopLength;

/***
 * Parse and set a configuration setting
 */
void parseSetting(String settingName, String settingValue) 
{

  settingName.trim();
  settingValue.trim();
  
  debug(settingName + ": " + settingValue);
  
  if (settingName.equalsIgnoreCase("volume")) {
    MASTER_VOLUME = settingValue.toFloat();  
    if (MASTER_VOLUME > 1) { 
      MASTER_VOLUME = 1;
    } else if (MASTER_VOLUME < 0) {
      MASTER_VOLUME = 0;
    }
  } else if (settingName.equalsIgnoreCase("lineout")) {
    LINEOUT = settingValue.toInt();
    if (LINEOUT < 13) {
      LINEOUT = 13;  
    } else if (LINEOUT > 31) {
      LINEOUT = 31;
    }
  } else if (settingName.equalsIgnoreCase("linein")) {
    LINEIN = settingValue.toInt();
    if (LINEIN < 0) {
      LINEIN = 0;  
    } else if (LINEIN > 15) {
      LINEIN = 15;
    }  
  } else if (settingName.equalsIgnoreCase("high_pass")) {
    HIPASS = settingValue.toInt();
    if (HIPASS < 0) { 
      HIPASS = 0;
    } else if (HIPASS > 1) {
      HIPASS = 1;
    }
  } else if (settingName.equalsIgnoreCase("mic_gain")) {
    MIC_GAIN = settingValue.toInt();  
  } else if (settingName.equalsIgnoreCase("button_click")) {
    BUTTON_WAV = settingValue;
  } else if (settingName.equalsIgnoreCase("startup")) {
    STARTUP_WAV = settingValue;
  } else if (settingName.equalsIgnoreCase("loop")) {
    LOOP_WAV = settingValue;
  } else if (settingName.equalsIgnoreCase("noise_gain")) {
    NOISE_GAIN = settingValue.toFloat();
  } else if (settingName.equalsIgnoreCase("button_gain")) {
    BUTTON_GAIN = settingValue.toFloat();
  } else if (settingName.equalsIgnoreCase("voice_gain")) {
    VOICE_GAIN = settingValue.toFloat();
  } else if (settingName.equalsIgnoreCase("effects_gain")) {
    EFFECTS_GAIN = settingValue.toFloat();
  } else if (settingName.equalsIgnoreCase("loop_gain")) {
    LOOP_GAIN = settingValue.toFloat();
    if (LOOP_GAIN < 0 or LOOP_GAIN > 32767) {
      LOOP_GAIN = 4;
    }
  } else if (settingName.equalsIgnoreCase("silence_time")) {
    SILENCE_TIME = settingValue.toInt();
  } else if (settingName.equalsIgnoreCase("voice_start")) {
    VOICE_START = settingValue.toFloat();
  } else if (settingName.equalsIgnoreCase("voice_stop")) {  
    VOICE_STOP = settingValue.toFloat();
  } else if (settingName.equalsIgnoreCase("input")) {
      AUDIO_INPUT = settingValue.toInt();
      if (AUDIO_INPUT > 1) {
        AUDIO_INPUT = 1;
      } else if (AUDIO_INPUT < 0) {
        AUDIO_INPUT = 0;
      }
  } else if (settingName.equalsIgnoreCase("eq")) {
    EQ_TYPE = settingValue.toInt();
    if (!EQ_TYPE or EQ_TYPE < 0 or EQ_TYPE > 3) {
      EQ_TYPE = FLAT_FREQUENCY;
    }
  } else if (settingName.equalsIgnoreCase("eq_bands")) {
    // clear bands and prep for setting
    for (int i = 0; i < EQ_BANDS_SIZE; i++) {
      EQ_BANDS[i] = 0;
    }
    // convert string to char array
    char buf[settingValue.length()+2];
    settingValue.toCharArray(buf, settingValue.length()+2);
    char * band;
    band = strtok(buf, ",");
    int i = 0;
    while (band && i < EQ_BANDS_SIZE) {
      EQ_BANDS[i] = atof(band);
      i++;
      band = strtok(NULL, ",");
    }
  } else if (settingName.equalsIgnoreCase("bitcrushers")) {
    char buf[settingValue.length()+2];
    settingValue.toCharArray(buf, settingValue.length()+2);
    char * token;
    token = strtok(buf, ",");
    int i = 0;
    while (token && i < 5) {
      BITCRUSHER[i] = atoi(token);
      i++;
      token = strtok(NULL, ",");
    }
  } else if (settingName.equalsIgnoreCase("button_pin")) {
    BUTTON_PIN = settingValue.toInt();
  } else if (settingName.equalsIgnoreCase("debug")) {
    if (settingValue.toInt() > 0) {
      DEBUG = true;
    } else {
      DEBUG = false;
    }
  }
}

/***
 * Converts all in-memory settings to string
 */
String settingsToString(boolean save) 
{
  String result = "";
  if (save) {
    result += "TKTalkie v2.0 www.tktalkie.com\n";
  }
  if (MASTER_VOLUME > 0) {
    result += "[volume=" + String(MASTER_VOLUME, 4).trim() + "]\n";
  }
  if (save) {
    result += "# Line-In level. Valid values are 0 to 15\n";
  }
  result += "[linein=" + String(LINEIN).trim() + "]\n";
  if (save) {
    result += "# Line-Out level output. Valid values are 13 to 31\n";
  }
  result += "[lineout=" + String(LINEOUT).trim() + "]\n";
  if (save) {
    result += "# sound to play when TKTalkie is started\n";
  }
  result += "[startup=" + STARTUP_WAV + "]\n";
  if (save) {
    result += "# chatter loop settings\n";
  }
  result += "[loop=" + LOOP_WAV + "]\n";
  if (save) {
    result += "# 0 to 32767, 1 is pass-thru, below 1 attenuates signal\n";
  }
  result += "[loop_gain=" + String(LOOP_GAIN, 4).trim() + "]\n";
  if (save) {
    result += "# VOICE SETTINGS\n";
     result += "# 0 to 32767, 1 is pass-thru, below 1 attenuates signal\n";
  }
  if (save) {
    result += "# turn high-pass filter on (1) or off (0)";
  }
  result += "[high_pass=" + String(HIPASS).trim() + "]\n";
  result += "[voice_gain=" + String(VOICE_GAIN, 4).trim() + "]\n";
  result += "[voice_start=" + String(VOICE_START, 4).trim() + "]\n";
  result += "[voice_stop=" + String(VOICE_STOP, 4).trim() + "]\n";
  result += "[silence_time=" + String(SILENCE_TIME) + "]\n";
  if (save) {
    result += "# PTT (Push-To-Talk) SETTINGS\n";
  }
  result += "[button_pin=" + String(BUTTON_PIN) + "]\n";
  result += "[button_click=" + BUTTON_WAV + "]\n";
  if (save) {
    result += "# 0 to 32767, 1 is pass-thru, below 1 attenuates signal\n";
  }
  result += "[button_gain=" + String(BUTTON_GAIN, 4).trim() + "]\n";
  if (save) {
    result += "# MICROPHONE/LINE-IN SETTINGS\n";
    result += "# input settings (1 = microphone, 0 = line-in)\n";
  }
  result += "[input=" + String(AUDIO_INPUT) + "]\n";
  if (save) {
    result += "# 0 to 63\n";
  }
  result += "[mic_gain=" + String(MIC_GAIN) + "]\n"; 
  if (save) {
    result += "# SOUND EFFECTS (STATIC BURSTS, ETC.)\n";
    result += "# 0 to 32767, 1 is pass-thru, below 1 attenuates signal\n";
  }
  
  result += "[effects_gain=" + String(EFFECTS_GAIN, 4).trim() + "]\n";
  if (save) {
    result += "# EQUALIZER SETTINGS\n";
    result += "# 0 = flat (none, 1 = parametric, 2 = bass/treble, 3 = graphic\n";
  }
  result += "[eq=" + String(EQ_TYPE) + "]\n";
  if (save) {
    result += "# for parametric/graphic = 5 bands, for bass/treble = 3 bands\n";
    result += "# bands are low to high: -1 (-11.75dB to 1 +12dB)\n";    
  }
  result += "[eq_bands=" + arrayToString(EQ_BANDS, EQ_BANDS_SIZE) + "]\n";
  if (save) {
    result += "# BITCRUSHER SETTINGS - VOCAL EFFECTS\n";
    result += "# Format = bits1,rate1,bits2,rate2\n";
    result += "# Set to 16,41000,16,41000 to just pass-thru (disable)\n";
  }
  result += "[bitcrushers=" + arrayToString(BITCRUSHER, BITCRUSHER_SIZE) + "]\n";
  if (save) {
    result += "# PINK NOISE GENERATOR\n";
    result += "# 0 to 32767, 1 is pass-thru, below 1 attenuates signal\n";
  }
  result += "[noise_gain=" + String(NOISE_GAIN, 4).trim() + "]\n";
  result += "[debug=" + String((int)DEBUG) + "]\n";
  return result;
}

/**
 * Backup settings to specified file
 */
boolean saveSettings(String filename) 
{
  boolean result = false;

  File myFile = openFile("BACKUP.TMP", FILE_WRITE);
  if (myFile) {
    char c;
    while (myFile.available()) {
      c = myFile.read();
      myFile.write(c);
    }
    myFile.close();

    myFile = openFile(filename, FILE_WRITE);
    if (myFile) {
      String settings = settingsToString(true);
      myFile.println(settings);
      myFile.close();
      result = true;
      Serial.println("Settings saved to " + filename);
    } else {
      Serial.println("ERROR SAVING TO: " + filename);
    }
    
  } else {
    Serial.println("ERROR CREATING TEMP BACKUP FILE");
  }
  
  
  return result;
}

/**
 * Save settings to default settings file
 */
boolean saveSettings() 
{
  return saveSettings(SETTINGS_FILE);
}

/***
 * Read settings from specified file
 */
boolean loadSettings(String filename) 
{
  boolean result = false;
  char character;
  String settingName;
  String settingValue;
  File myFile = openFile(filename);
  if (myFile) {
    debug("Opening settings file " + filename);
    while (myFile.available()) {
      character = myFile.read();
      while ((myFile.available()) && (character != '[')) {
        character = myFile.read();
      }
      character = myFile.read();
      while ((myFile.available()) && (character != '='))  {
        settingName = settingName + character;
        character = myFile.read();
      }
      character = myFile.read();
      while ((myFile.available()) && (character != ']'))  {
        settingValue = settingValue + character;
        character = myFile.read();
      }
      if  (character == ']')  {
        parseSetting(settingName, settingValue);
        // Reset Strings
        settingName = "";
        settingValue = "";
      }
    }
    myFile.close();
    result = true;
  } else {
    // if the file didn't open, print an error:
    debug("ERROR opening settings file " + filename);
  }
  return result;
}

/**
 * Load defult settings file
 */
boolean loadSettings() 
{
  return loadSettings(SETTINGS_FILE);
}

/***
 * Apply settings
 */
void applySettings() 
{
  // Turn on the 5-band graphic equalizer (there is also a 7-band parametric...see the Teensy docs)
  audioShield.eqSelect(EQ_TYPE);
  switch (EQ_TYPE) {
    case PARAMETRIC_EQUALIZER:
    case GRAPHIC_EQUALIZER:
      // Bands (from left to right) are: Low, Low-Mid, Mid, High-Mid, High.
      // Valid values are -1 (-11.75dB) to 1 (+12dB)
      // The settings below pull down the lows and highs and push up the mids for 
      // more of a "tin-can" sound.
      audioShield.eqBands(EQ_BANDS[0], EQ_BANDS[1], EQ_BANDS[2], EQ_BANDS[3], EQ_BANDS[4]);
      break;
    case TONE_CONTROLS:
      // Just bass and treble
      audioShield.eqBands(EQ_BANDS[0], EQ_BANDS[1]);
      break;
    default:
      // FLAT_FREQUENCY - EQ disabled
      break;
  }
  // tell the audio shield which input to use
  audioShield.inputSelect(AUDIO_INPUT);
  // adjust the gain of the input
  // adjust this as needed
  if (AUDIO_INPUT == 0) {
    audioShield.lineInLevel(LINEIN);
  } else {
    audioShield.micGain(MIC_GAIN);
  }  
  // You can modify these values to process the voice 
  // input.  See the Teensy bitcrusher demo for details.
  bitcrusher1.bits(BITCRUSHER[0]);
  bitcrusher1.sampleRate(BITCRUSHER[1]);
  bitcrusher2.bits(BITCRUSHER[2]);
  bitcrusher2.sampleRate(BITCRUSHER[3]);
  // Bitcrusher 1 input (fed by mic/line-in)
  voiceMixer.gain(0, VOICE_GAIN);
  // Bitcrusher 2 input (fed by mic/line-in)
  voiceMixer.gain(1, VOICE_GAIN);
  // Pink noise channel
  voiceMixer.gain(2, NOISE_GAIN);
  // Feed from effects mixer
  voiceMixer.gain(3, 1);
  // stereo channels for SD card...adjust gain as 
  // necessary to match voice level
  effectsMixer.gain(0, EFFECTS_GAIN);
  effectsMixer.gain(1, EFFECTS_GAIN);
  // Button beep sound
  effectsMixer.gain(2, BUTTON_GAIN);
  // Feed from loop mixer
  effectsMixer.gain(3, 1);
  // chatter loop from SD card
  loopMixer.gain(0, LOOP_GAIN);
  loopMixer.gain(1, LOOP_GAIN);
  audioShield.volume(readVolume());
  audioShield.lineOutLevel(LINEOUT);
  if (HIPASS == 0) {
    audioShield.adcHighPassFilterDisable();
  } else {
    audioShield.adcHighPassFilterEnable();
  }
}

/***
 * recursively list all files on SD card
 */
String dirSep = "";

void listFiles(File dir) 
{
   while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       if (dirSep != "") {
          dirSep = dirSep.substring(0, dirSep.length()-1);
       }
       break;
     }
     if (entry.isDirectory()) {
       // If you DON'T want to recursively search directories, 
       // comment out this line.
       Serial.println(dirSep + "/" + (String)entry.name());
       dirSep += "-";
       loadFiles(entry);
     } else {
       // convert to string to make it easier to work with...
       Serial.println(dirSep + (String)entry.name());
     }
     entry.close();
     if (dirSep != "") {
       dirSep = dirSep.substring(0, dirSep.length()-1);
     }
   }
}

/***
 * Read the contents of the SD card and put any files staring with "TKT_" and ending 
 * with ".WAV" into the array.  It will recursively search directories.  
 */
void loadFiles(File dir) 
{
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
 * Play the specified sound effect from the SD card
 */
long playEffect(int player, String filename) 
{
  long len = 0;
  // Convert string to char array
  char buf[filename.length()+2];
  filename.toCharArray(buf, filename.length()+2);
  // Start playing the file.  This sketch continues to
  // run while the file plays.
  switch (player) {
    case LOOP_PLAYER:
      loopPlayer.play(buf);
      delay(10);
      len = loopPlayer.lengthMillis();
      break;
    default:
      effectsPlayer.play(buf);
      delay(10);
      len = effectsPlayer.lengthMillis();
      break;
  }
  debug("PLAYING " +  filename + " @ " + String(len) + " ms (" + String(len/1000) + " sec)");
  return len;
}

/***
 * This plays the specified WAV file from the SD Card
 */
void playFile(int player, String filename, int nextState)
{
  long l = playEffect(player, filename);
  if (l) {
    delay(l+100);
    STATE = nextState;
  }
}

/***
 * Play sound loop and set counters
 */
void playLoop() 
{
  loopLength = playEffect(LOOP_PLAYER, LOOP_WAV);
  loopMillis = 0;
}

/***
 * Play a random sound effect from the SD card
 */
void addSoundEffect()
{
  if (speaking == true) return;
  // generate a random number between 0 and the number of files read - 1
  int rnd = lastRnd;
  while (rnd == lastRnd) {
    rnd = random(0, wavCount);
  }
  lastRnd = rnd;
  // file names are stored as strings in the array
  String filename = wavFiles[rnd];
  // play the file
  playEffect(EFFECTS_PLAYER, filename);
}

/***
 * Check the optional volume pot for output level
 */
float readVolume()
{
    float vol = 0;
    if (MASTER_VOLUME) {
      audioShield.volume(MASTER_VOLUME);
      vol = MASTER_VOLUME;
    } else {
      // comment these lines if your audio shield does not have the optional volume pot soldered on
      vol = analogRead(15);
      vol = vol / 1023;
      audioShield.volume(vol);
    }
    if (vol > 1.0) {
      vol = 1.0;
    } else if (vol < 0) {
      vol = 0;
    }
    //debug("READ VOLUME: " + String(vol, 4));
    return vol;
}

/*** 
 * Check if the PTT button was pressed 
 */
boolean checkButton() 
{
  PTT.update();
  if (PTT.fell()) {
    playEffect(EFFECTS_PLAYER, BUTTON_WAV);
    return true;
  } else {
    return false;
  }
}

/***
 * This is played when switching from PTT back to Voice Activated mode.
 * You could also play a .WAV file here 
 */
void PTTWarning() 
{
  waveform1.frequency(660);
  waveform1.amplitude(0.5);
  delay(50);
  waveform1.amplitude(0);
  delay(50);
  waveform1.frequency(1320);
  waveform1.amplitude(0.5);
  delay(50);
  waveform1.amplitude(0);
  delay(50);
  waveform1.frequency(2640);
  waveform1.amplitude(0.5);
  delay(50);
  waveform1.amplitude(0);
}

/***
 * Turns the volume down on the chatter loop
 */
void loopOff() 
{
  loopMixer.gain(0, 0);
  loopMixer.gain(1, 0);
}

/***
 * Turns the volume up on the chatter loop
 */
void loopOn() 
{
  // gradually raise level to avoid pops 
  if (LOOP_GAIN > 1) {
    for (int i=0; i<=LOOP_GAIN; i++) {
      loopMixer.gain(2, i);
      loopMixer.gain(3, i);
    }
  }
  loopMixer.gain(0, LOOP_GAIN);
  loopMixer.gain(1, LOOP_GAIN);
}

/***
 * Turns off the voice channels on the mixer
 */
void voiceOff() 
{
  speaking = false;
  silent = false;
  stopped = 0;
  pink1.amplitude(0);
  voiceMixer.gain(0, 0);
  voiceMixer.gain(1, 0);
}

/***
 * Turns on the voice channels on the mixer
 */
void voiceOn() 
{
  speaking = true;
  silent = true;
  // Reset the "user is talking" timer
  stopped = 0;
  // pops are ok here ;)
  pink1.amplitude(NOISE_GAIN);
  voiceMixer.gain(0, VOICE_GAIN);
  voiceMixer.gain(1, VOICE_GAIN);
}

/***
 * Used for calibration wizard...takes sample readings from input
 */
void sampleMic(String prompt, float &avg, float &loBase, float &hiBase) {
  Serial.println(prompt);
  Serial.print("Listening in ");
  for (int i = 10; i >= 0; i--) {
    Serial.print(i);
    if (i > 0) {
      Serial.print(" > ");
      delay(1000);
    }
  }
  Serial.println("");
  loBase = 0;
  hiBase = 0;
  avg = 0;
  float total = 0;
  int count = 0;
  elapsedMillis timer = 0;
  Serial.print("Listening for 10 seconds...");
  voiceOn();
  while (timer < 10000) {
    if (rms1.available()) {
        // get the input amplitude...will be between 0.0 (no sound) and 1.0
        float val = rms1.read();
        if (val > 0) {
          count++;
          if (val < loBase) {
            loBase = val;
          }
          if (val > hiBase) {
            hiBase = val;
          }
          total += val;
        }
        delay(10);
    }
  }
  voiceOff();
  Serial.println("Done (" + String(count) + " samples)");
  avg = total/count;
}

/***
 * Calibration wizard
 */
void calibrate() 
{
  // disable normal operation
  STATE = BOOTING;  

  String recom = "";
  
  loopOff();

  Serial.println("CALIBRATING...Please make sure your microphone is on!");
  Serial.println("");

  float avg = 0;
  float loBase = 0;
  float hiBase = 0;
  
  sampleMic("Please speak normally into your microphone when the countdown reaches 0", avg, loBase, hiBase);
  Serial.println("Average Trigger level: " + String(avg, 4) + " (Low: " + String(loBase, 4) + " Peak: " + String(hiBase, 4) + ")");
  avg += .1;
  recom += "voice_start=" + String(avg, 4) + '\n';
  
  Serial.println("");
  sampleMic("Please leave your microphone on and keep silent so that we can get a baseline...", avg, loBase, hiBase);
  Serial.println("Average Baseline level: " + String(avg, 4) + " (Low: " + String(loBase, 4) + " Peak: " + String(hiBase, 4) + ")");
  Serial.println("");
  
  if (avg < 0.01) {
    avg = 0.01;
  }
  recom += "voice_off=" + String(avg, 4) + '\n';

  Serial.println("");
  Serial.println("RECOMMENDED SETTINGS:");
  Serial.println("Typically the baseline (voice_off) should not be less than .01");
  Serial.println("As a rule of thumb, the baseline (voice_off) should not be less than .01 and the");
  Serial.println("trigger level (voice_start) should be around .1 higher than the average reading.");
  Serial.println("You can also try setting it midway between the average and peak settings.");
  Serial.println("");
  Serial.println(recom);
  Serial.println("");

  loopOn();
  
  STATE = RUNNING;
  
}

/***
 * Initial setup...runs only once when the board is turned on.
 */
void setup() 
{
  STATE = BOOTING;
  
  // You really only need the Serial connection 
  // for output while you are developing, so you 
  // can uncomment this and use Serial.println()
  // to write messages to the console.
  Serial.begin(57600);
  
  delay(500);
  
  Serial.println("");
  Serial.println("TKTalkie (c) 2016 Because...Interwebs!");
  Serial.println("www.TKTalke.com");
  Serial.println("Type 'debug=1' [ENTER] to see messages!");
  Serial.println("");
  
  // Always allocate memory for the audio shield!
  AudioMemory(48);
  
  // turn on audio shield
  audioShield.enable();
  
  // disable volume and outputs during setup to avoid pops
  audioShield.muteLineout();
  audioShield.muteHeadphone();
  audioShield.volume(0);
  
  // turn on post processing
  audioShield.audioPostProcessorEnable();

  // Activate the onboard pre-processor
  //audioShield.audioPreProcessorEnable();
  
  // Check SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // Serial.println("Unable to access the SD card");
  } else {
    // Read the contents of the SD card
    File root = openFile("/");
    loadFiles(root);
    root.close();
    loadSettings();
    delay(100);
    STATE = BOOTED;
  }
  // this just makes sure we get truly random numbers each time
  // when choosing a file to play from the list later on...
  randomSeed(analogRead(0));
  // Initialize PTT button
  if (BUTTON_PIN && BUTTON_PIN > 0) {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    PTT.attach(BUTTON_PIN);
    PTT.interval(15);
  }
  // Initialize sound generator for warning tones when we switch modes from PTT back to Voice Activated
  waveform1.begin(WAVEFORM_SINE);
}

/***
 * Main program loop
 */
void loop() 
{

   if (Serial.available() > 0) { 
     String str = Serial.readStringUntil('\n');   
     if (str.equalsIgnoreCase("save")) {
        saveSettings();
     } else if (str.equalsIgnoreCase("load")) {
        loadSettings();  
        applySettings();
     } else if (str.equalsIgnoreCase("save")) {
        saveSettings();  
     } else if (str.equalsIgnoreCase("backup")) {
        saveSettings(BACKUP_FILE); 
     } else if (str.equalsIgnoreCase("restore")) {
        loadSettings(BACKUP_FILE);    
        applySettings();
     } else if (str.equalsIgnoreCase("settings")) {
        showSettings();
     } else if (str.equalsIgnoreCase("files")) {
        File root = openFile("/");
        listFiles(root);
        root.close();
     } else if (str.equalsIgnoreCase("help")) {
        showHelp();
     } else if (str.equalsIgnoreCase("calibrate")) {
        calibrate();
     } else {
        int sep = str.indexOf("=");
        if (sep > 0) {
          String settingName = str.substring(0, sep);
          String settingValue = str.substring(sep+1);
          if (settingName.equalsIgnoreCase("save")) {
             saveSettings(settingValue);  
          } else if (settingName.equalsIgnoreCase("load")) {
             loadSettings(settingValue);
             applySettings();
          } else if (settingName.equalsIgnoreCase("play")) {
             playEffect(EFFECTS_PLAYER, settingValue);
          } else { 
            parseSetting(settingName, settingValue);
            applySettings();
            if (settingName.equalsIgnoreCase("loop")) {
              playLoop();
            }
          }
        }
     }
  }
   
  if (BUTTON_PIN && button_initialized == false) {
    button_initialized = checkButton();
    if (button_initialized) {
      ms = 0;
      // turn voice on with background noise
      voiceOn();
    }
  } else {
    PTT.update();
  }
  
  switch (STATE) {
    case BOOTING: 
      // Do nothing while the config is being loaded
      // Also used while calibrating
      break;
    case BOOTED:
      // config finished...apply the settings and play startup file
      // volume level is 0.0 to 1.0
      readVolume();
      applySettings();
      STATE = STARTUP;
      break;
    case STARTUP:
      // do nothing while startup file is being played
      audioShield.unmuteLineout();
      audioShield.unmuteHeadphone();
      STATE = STARTING;
      playFile(EFFECTS_PLAYER, STARTUP_WAV, STARTED);
      break;
    case STARTING:
      // do nothing while the startup sound plays
      break;
    case STARTED:
      STATE = STARTING;
      // startup file finished, load chatter loop
      playLoop();
      STATE = RUNNING;
      break;
    case RUNNING:
      // normal program operation
      run();
      break;
  }
}

/***
 * Called from main loop
 */
void run() {

  // check loop
  if (loopLength && loopMillis > loopLength) {
      playLoop();
  }
  
  if (BUTTON_PIN && button_initialized) {

    debug("BUTTON");
    // Check if there is silence.  If not, set a flag so that
    // we don't switch back to Voice Activated mode accidentally ;)
    if (speaking == true && silent == true) {
        if (rms1.available()) {
          float val = rms1.read();
          // This check is here to make sure we don't get false readings
          // when the button click noise is played which would prevent 
          // the switch back to Voice Activated mode
          if ((val-VOICE_STOP) >= .1) {
            silent = false;
          }
        }
    }

    // Switch back to Voice Activated mode if:
    //    1.  PTT button was pushed
    //    2.  There has been silence for at least 2 seconds
    // NOTE:  If you start talking before the 2 second time limit
    //        it will NOT switch back...or if you talk and pause for 
    //        2 seconds or more it will NOT switch back.
    if (speaking == true && silent == true && stopped > 2000) {
      voiceOff();
      button_initialized = false;
      PTTWarning();
      return;
    }

    // Button press
    
    if (PTT.fell()) {
      playEffect(EFFECTS_PLAYER, BUTTON_WAV);
      //ms = 0;
      voiceOn();
    }
    
    // Button release
    if (PTT.rose()) {
      voiceOff();
      // Random comm sound
      addSoundEffect();
    }
    
  } else {

    //if (ms > 24) {
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
        if (val >= VOICE_START) {

           debug("VOICE START: " + String(val, 4));
           
          // If user is not currently speaking, then they just started talking :)
          if (speaking == false) {

            loopOff();
            voiceOn();
        
          }
          
        } else if (speaking == true) {
          
            if (val < VOICE_STOP) {
  
              // If the user has stopped talking for at least the required "silence" time and 
              // the mic/line input has fallen below the minimum input threshold, play a random 
              // sound from the card.  NOTE:  You can adjust the delay time to your liking...
              // but it should probably be AT LEAST 1/4 second (250 milliseconds.)
  
              if (stopped >= SILENCE_TIME) {
                
                voiceOff();
                
                // play random sound effect
                addSoundEffect();

                loopOn();
  
              }
  
            } else {
                
                // Reset the "silence" counter 
                stopped = 0;
                
            }
  
          }
        
       }

    //}

  }
  
  readVolume();
  
}

/*********************************************************
 * UTILITY FUNCTIONS
 *********************************************************/
 
/**
 * Convert array of int to comma-delimited string 
 */
String arrayToString(int arr[], int len) 
{
  String result = "";
  for (int i =0 ; i < len; i++) {
    result += String(arr[i]);
    if (i < len-1) {
      result += ",";  
    }
  }
  return result;
}

/**
 * Convert array of float to comma-delimited string
 */
String arrayToString(float arr[], int len) 
{
  String result = "";
  for (int i = 0 ; i < len; i++) {
    result += String(arr[i]);
    if (i < len-1) {
      result += ",";  
    }
  }
  return result;
}

/**
 * Open a file on the SD card in the specified mode.
 */
File openFile(String filename, int mode) 
{
  char buf[filename.length()+2];
  filename.toCharArray(buf, filename.length()+2);
  // Thanks to TanRu !
  if (mode == FILE_WRITE) {
    if (SD.exists(buf)) {
      SD.remove(buf);
    }
  }
  return SD.open(buf, mode);  
}

/**
 * Open a file on the SD card, defaulting to READ mode
 */
File openFile(String filename) 
{
  return openFile(filename, FILE_READ);
}

/***
 * Show a list of commands
 */
void showHelp() {
  printDivider();
  Serial.println("help               Show this help screen.");
  Serial.println("files              Show a list of files on SD card.");
  Serial.println("settings           Show current settings.");
  Serial.println("load               Load settings from default file (TKCONFIG.TXT)");
  Serial.println("save               Save current settings to default file (TKCONFIG.TXT)");
  Serial.println("load={filename}    Load settings from specified file (no braces!)");
  Serial.println("                   Use 'save' command to save to default file");
  Serial.println("save={filename}    Save current settings to specified file (no braces!)"); 
  Serial.println("play={filename}    Play specified .WAV file (no braces, use full file name)");
  Serial.println("");
  Serial.println("backup             Quick backup of settings file to SETTINGS.BAK");
  Serial.println("restore            Quick restore from SETTINGS.BAK");
  Serial.println("");
  Serial.println("CALIBRATING");
  Serial.println("To help set Voice-Activation thresholds, simply enter the command 'calibrate'");
  Serial.println("then follow the on-screen prompts.");
  Serial.println("");
  Serial.println("CHANGING SETTINGS");
  Serial.println("You can change settings on-the-fly (and save them) by entering them in the same format");
  Serial.println("as they are in the settings file.  For example:");
  Serial.println("");
  Serial.println("loop_gain=7 [ENTER]");
  Serial.println("");
  Serial.println("Changes take effect IMMEDIATELY but will be lost if you don't use the 'save' command.");
  Serial.println("");
  Serial.println("For more information, please visit www.TKTalkie.com");
  printDivider();
}

/***
 * Prints a line...
 */
void printDivider() {
  Serial.println("--------------------------------------------------------------------------------");
}

/***
 * Print out the current settings
 */
void showSettings() {
  Serial.println("");
  printDivider();
  Serial.println(settingsToString(false));
  printDivider();
  Serial.println("");
}

/***
 * Prints debug messages
 */
void debug(String text) 
{
  if (DEBUG == true) {
    Serial.println(text);
  }
}

// END
