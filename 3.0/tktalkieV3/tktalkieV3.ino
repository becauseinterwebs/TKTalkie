/****
 * TK TALKIE by TK-81113 (Brent Williams) <becauseinterwebs@gmail.com>
 * www.tktalkie.com / www.tk81113.com 
 * 
 * Version 3.0 (Apr 10, 2016)
 *
 * WhiteArmor.net User ID: lerxstrulz
 * 
 * This sketch is meant to use a Teensy 3.2 with a Teensy Audio Shield and 
 * reads sounds from an SD card and plays them after the user stops talking 
 * to simulate comm noise such as clicks and static. This version adds a lot  
 * of new features, including the ability to be controlled via a mobile app 
 * and Bluetooth.  This release also introduces memory optimizations and other 
 * improvements.
 * 
 * You are free to use this code in your own projects, provided they are for 
 * personal, non-commercial use.
 * 
 * The audio components and connections were made using the GUI tool 
 * available at http://www.pjrc.com/teensy/gui.  If you want to modify 
 * the signal path, add more effects, etc., you can copy and paste the 
 * code marked by 'GUITool' into the online editor at the above URL.
 * 
 * WHAT'S NEW:
 * 
 * V3.0 (2017)s
 *  1.  Modified to be able to communicate via Bluetooth Low Energy (BLE) 
 *      serial adapter with mobile app for control of settings and profile 
 *      switching.
 *  2.  Began optimizations on usage of C-type strings to reduce memory usage.
 *  3.  Added new commands specific to BLE adapter usage.
 *  4.  Added new commands to manage multiple configuration profiles.
 *  5.  Reorganized SD card files and folders
 *  
 *  Please visit http://www.tktalkie.com/changelog for prior changes.
 */
 
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce2.h>
#include "globals.h"

/**
 * Emit a warning tone
 */
void beep(const int times = 1)
{
  audioShield.unmuteHeadphone();
  audioShield.unmuteLineout();
  for (int i=0; i<times; i++) {
    waveform1.frequency(720);
    waveform1.amplitude(0.7);
    delay(100);
    waveform1.frequency(1440);
    delay(50);
    waveform1.amplitude(0);
    delay(350);
  }
}

void upcase(char *str)
{
   int i = 0;
   if (strcasecmp(str, "") == 0) {
    return;
   }
   while (str[i] != '\0') {
    toupper(str[i]);
    i++;
   }
}

/**
 * Makes sure paths start and end with /
 */
void fixPath(char *path)
{
   if (strcasecmp(path, "") == 0) {
    return;
   }

   if (path[0] != '/') {
     char buf[SETTING_ENTRY_MAX] = "/";
     strcat(buf, path);
     strcpy(path, buf);
   }

   int i = 1;
   while (path[i] != '\0' && i < SETTING_ENTRY_MAX) {
    i++;
   }
   
   if (path[i-1] != '/') {
    path[i]   = '/';
    path[i+1] = '\0';
   }

}

/***
 * recursively list all files on SD card
 */
String dirSep = "";

int listDirectories(const char *path, char directories[][SETTING_ENTRY_MAX])
{
   int index = 0;
   File dir = SD.open(path);
   dir.rewindDirectory();
   Serial.print("READ DIR: ");
   Serial.println(path);
   while(true && index < MAX_FILE_COUNT) {
     File entry = dir.openNextFile();
     if (! entry) {
      Serial.println("No More Files");
       // no more files
       if (dirSep != "") {
          dirSep = dirSep.substring(0, dirSep.length()-2);
       }
       break;
     }
     if (entry.isDirectory()) {
       Serial.print(" -> DIR -> ");
       Serial.println(entry.name());
       char *ret = strstr(entry.name(), "~");
       if (ret == NULL) {
         Serial.println(entry.name());
         strcpy(directories[index], entry.name());
         dirSep += "  ";
         index += listDirectories(entry.name(), directories);
         index++;
       }
     }  
     entry.close();
   }
   return index;
}

/**
 * Return a directory listing.
 * If filter is specified, only file names containing
 * the filter text are returned.
 */
 int listFiles(const char *path, char files[][SETTING_ENTRY_MAX], int max, const char *match, boolean recurse, boolean echo) 
{
  char filter[SETTING_ENTRY_MAX];
  strcpy(filter, match);
  upcase(filter);
  boolean checkFilter = (strcasecmp(filter, "") == 0) ? false : true;
  if (checkFilter) {
    debug(F("Filter: %s\n"), filter);
  }
  int index = 0;
  File dir = SD.open(path);
  if (!dir) {
    return 0;
  }
  dir.rewindDirectory();
  while(true && index < max) {
     File entry = dir.openNextFile();
     if (! entry) {
       // no more files
       if (dirSep != "") {
          dirSep = dirSep.substring(0, dirSep.length()-2);
       }
       break;
     }
     if (entry.isDirectory() && recurse == true) {
       // Filter out folders with ~ (backups)
       char *ret = strstr(entry.name(), "~");
       if (ret == NULL) {
         if (echo || DEBUG) {
          Serial.print(dirSep);
          Serial.print(entry.name());
          Serial.println("/");
         }
         dirSep += "  ";
         index += listFiles(entry.name(), files, max-index, filter, recurse, echo);
       }
     } else {
        char *fname = entry.name();
        upcase(fname);
        // Filter out filenames with ~ (backups)
        char *ret = strstr(fname, "~");
        if (ret == NULL) {
            if (checkFilter) {
              ret = strstr(fname, filter);
              if (ret == NULL) {
                fname[0] = '\0';
              }
            }
        } else {
          fname[0] ='\0';
        }
       if (strcasecmp(fname, "") != 0) {
            if (echo || DEBUG) {
              Serial.println(dirSep + entry.name());
            }
            if (index < max) {
              strcpy(files[index], entry.name());
              index += 1;
            }
       }
     }
     entry.close();
   }
   return index;
}

/***
 * Read the contents of the SD card and put any files ending with ".WAV" 
 * into the array.  It will recursively search directories.  
 */
void loadSoundEffects() 
{
  SOUND_EFFECTS_COUNT = listFiles(EFFECTS_DIR, SOUND_EFFECTS, MAX_FILE_COUNT, SOUND_EXT, false, false);
  debug(F("%d Sound effects loaded\n"), SOUND_EFFECTS_COUNT);
}

/***
 * Play the specified sound effect from the SD card
 */
long playSoundFile(int player, char *filename) 
{

  if (strcasecmp(filename, "") == 0) {
    debug(F("Exit play sound -> blank file name\n"));
    return 0;
  }
  char *ret = strstr(filename, ".");
  if (ret == NULL) {
    char ext[5];
    strcpy(ext, SOUND_EXT);
    strcat(filename, ext);
  }
  debug(F("Play sound file %s on player %d\n"), filename, player);
  long len = 0;
  switch (player) {
    case LOOP_PLAYER:
      loopPlayer.stop();
      loopPlayer.play(filename);
      delay(10);
      len = loopPlayer.lengthMillis();
      break;
    default:
      effectsPlayer.play(filename);
      delay(10);
      len = effectsPlayer.lengthMillis();
      break;
  }
  debug(F("Sound File Length: %d\n"), len);
  return len;
}

/**
 * Shortcut to play a sound from the SOUNDS directory
 */
long playSound(const char *filename)
{
  if (strcasecmp(filename, "") == 0) {
    return 0;
  }
  char buf[100];
  strcpy(buf, SOUNDS_DIR);
  strcat(buf, filename);
  return playSoundFile(EFFECTS_PLAYER, buf);
}

/**
 * Shortcut to play a sound from the EFFECTS directory
 */
long playEffect(const char *filename)
{
  if (strcasecmp(filename, "") == 0) {
    return 0;
  }
  char buf[100];
  strcpy(buf, EFFECTS_DIR);
  strcat(buf, filename);
  return playSoundFile(EFFECTS_PLAYER, buf);
}

/***
 * Play sound loop and set counters
 */
void playLoop() 
{
  loopLength = 0;
  if (strcasecmp(LOOP_WAV, "") != 0) {
    char buf[100];
    strcpy(buf, LOOP_DIR);
    strcat(buf, LOOP_WAV);
    loopLength = playSoundFile(LOOP_PLAYER, buf);
  }
  loopMillis = 0;
}

/***
 * Play a random sound effect from the SD card
 */
void addSoundEffect()
{
  if (speaking == true || SOUND_EFFECTS_COUNT < 1) return;
  // generate a random number between 0 and the number of files read - 1
  int rnd = 0;
  int count = 0;
  rnd = lastRnd;
  while (rnd == lastRnd && count < 50) {
   rnd = random(0, SOUND_EFFECTS_COUNT);
   count++;
  }
  lastRnd = rnd;
  // play the file
  playEffect(SOUND_EFFECTS[rnd]);
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
    return vol;
}

/*** 
 * Check if the PTT button was pressed 
 */
boolean checkButton() 
{
  PTT.update();
  if (PTT.fell()) {
    playEffect(BUTTON_WAV);
    return true;
  } else {
    return false;
  }
}

/***
 * This is played when switching from PTT back to Voice Activated mode.
 * It is also played when a device connects via Bluetooth. 
 */
void connectSound() 
{
  int freqs[3] = { 660, 1320, 2640 };
  for (int i=0; i<3; i++) {
    waveform1.frequency(freqs[i]);
    waveform1.amplitude(0.5);
    delay(100);
  }
  waveform1.amplitude(0);
}

/***
 * This is played when a mobile device connects via Bluetooth 
 */
void disconnectSound() 
{
  int freqs[3] = { 2640, 1320, 660 };
  for (int i=0; i<3; i++) {
    waveform1.frequency(freqs[i]);
    waveform1.amplitude(0.5);
    delay(100);
  }
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
void sampleMic(const char *prompt, float &avg, float &loBase, float &hiBase) {
  Serial.println(prompt);
  Serial.print(F("Listening in "));
  for (int i = 10; i >= 0; i--) {
    Serial.print(i);
    if (i > 0) {
      Serial.print(F(" > "));
      delay(1000);
    }
  }
  Serial.println(F(""));
  loBase = 0;
  hiBase = 0;
  avg = 0;
  float total = 0;
  int count = 0;
  elapsedMillis timer = 0;
  Serial.print(F("Listening for 10 seconds..."));
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
  Serial.print("Done (");
  Serial.print(count);
  Serial.println(" samples)");
  avg = total/count;
}

/***
 * Calibration wizard
 */
void calibrate() 
{

  // disable normal operation
  STATE = STATE_BOOTING;  

  char recom[SETTING_ENTRY_MAX*2];
  
  loopOff();

  Serial.println(F("CALIBRATING...Please make sure your microphone is on!"));
  Serial.println(F(""));

  float avg = 0;
  float loBase = 0;
  float hiBase = 0;

  char buf[SETTING_ENTRY_MAX];

  sampleMic("Please speak normally into your microphone when the countdown reaches 0", avg, loBase, hiBase);
  Serial.print("Average Trigger level: ");
  dtostrf(avg, 0, 4, buf);
  Serial.print(buf);
  Serial.print(" (Low: ");
  memset(buf, 0, sizeof(buf));
  dtostrf(loBase, 0, 4, buf);
  Serial.print(buf);
  Serial.print(" Peak: ");
  memset(buf, 0, sizeof(buf));
  dtostrf(hiBase, 0, 4, buf);
  Serial.print(buf);
  Serial.println(")");
  memset(buf, 0, sizeof(buf));
  avg += .01;
  strcpy(recom,  "voice_start=");
  dtostrf(avg, 0, 4, buf);
  strcat(recom, buf);
  strcat(recom, "\n");
  memset(buf, 0, sizeof(buf));
  
  Serial.println("");
  sampleMic("Please leave your microphone on and keep silent so that we can get a baseline...", avg, loBase, hiBase);
  Serial.println("Average Baseline level: ");
  dtostrf(avg, 0, 4, buf);
  Serial.print(buf);
  Serial.print(" (Low: ");
  memset(buf, 0, sizeof(buf));
  dtostrf(loBase, 0, 4, buf);
  Serial.print(buf);
  Serial.print(" Peak: ");
  memset(buf, 0, sizeof(buf));
  dtostrf(hiBase, 0, 4, buf);
  Serial.print(buf);
  Serial.println(")");
  memset(buf, 0, sizeof(buf));
  
  Serial.println("");
  
  if (avg < 0.01) {
    avg = 0.01;
  }
  strcat(recom, "voice_off=");
  dtostrf(avg, 0, 4, buf);
  strcat(recom, buf);
  strcat(recom, "\n");
  memset(buf, 0, sizeof(buf));

  Serial.println(F(""));
  showFile("CALIBRATE.TXT");
  Serial.println(F(""));
  Serial.println(recom);
  Serial.println(F(""));

  loopOn();
  
  STATE = STATE_RUNNING;
  
}

/***
 * Parse and set a configuration setting
 */
void parseSetting(const char *settingName, char *settingValue) 
{

  debug(F("Parse Setting: %s = %s\n"), settingName, settingValue);
  
  if (strcasecmp(settingName, "name") == 0) {
    memset(PROFILE_NAME, 0, sizeof(PROFILE_NAME));
    strcpy(PROFILE_NAME, settingValue);
  } else if (strcasecmp(settingName, "volume") == 0) {
    MASTER_VOLUME = atof(settingValue);  
    if (MASTER_VOLUME > 1) { 
      MASTER_VOLUME = 1;
    } else if (MASTER_VOLUME < 0) {
      MASTER_VOLUME = 0;
    }
  } else if (strcasecmp(settingName, "lineout") == 0) {
    LINEOUT = atoi(settingValue);
    if (LINEOUT < 13) {
      LINEOUT = 13;  
    } else if (LINEOUT > 31) {
      LINEOUT = 31;
    }
  } else if (strcasecmp(settingName, "linein") == 0) {
    LINEIN = atoi(settingValue);
    if (LINEIN < 0) {
      LINEIN = 0;  
    } else if (LINEIN > 15) {
      LINEIN = 15;
    }  
  } else if (strcasecmp(settingName, "high_pass") == 0) {
    HIPASS = atoi(settingValue);
    if (HIPASS < 0) { 
      HIPASS = 0;
    } else if (HIPASS > 1) {
      HIPASS = 1;
    }
  } else if (strcasecmp(settingName, "mic_gain") == 0) {
    MIC_GAIN = atoi(settingValue);  
  } else if (strcasecmp(settingName, "button_click") == 0) {
    memset(BUTTON_WAV, 0, sizeof(BUTTON_WAV));
    strcpy(BUTTON_WAV, settingValue);
  } else if (strcasecmp(settingName, "startup") == 0) {
    memset(STARTUP_WAV, 0, sizeof(STARTUP_WAV));
    strcpy(STARTUP_WAV, settingValue);
  } else if (strcasecmp(settingName, "loop") == 0) {
    memset(LOOP_WAV, 0, sizeof(LOOP_WAV));
    strcpy(LOOP_WAV, settingValue);
  } else if (strcasecmp(settingName, "noise_gain") == 0) {
    NOISE_GAIN = atof(settingValue);
  } else if (strcasecmp(settingName, "voice_gain") == 0) {
    VOICE_GAIN = atof(settingValue);
  } else if (strcasecmp(settingName, "effects_gain") == 0) {
    EFFECTS_GAIN = atof(settingValue);
  } else if (strcasecmp(settingName, "loop_gain") == 0) {
    LOOP_GAIN = atof(settingValue);
    if (LOOP_GAIN < 0 or LOOP_GAIN > 32767) {
      LOOP_GAIN = 4;
    }
  } else if (strcasecmp(settingName, "silence_time") == 0) {
    SILENCE_TIME = atoi(settingValue);
  } else if (strcasecmp(settingName, "voice_start") == 0) {
    VOICE_START = atof(settingValue);
  } else if (strcasecmp(settingName, "voice_stop") == 0) {  
    VOICE_STOP = atof(settingValue);
  } else if (strcasecmp(settingName, "input") == 0) {
      AUDIO_INPUT = atoi(settingValue);
      if (AUDIO_INPUT > 1) {
        AUDIO_INPUT = 1;
      } else if (AUDIO_INPUT < 0) {
        AUDIO_INPUT = 0;
      }
  } else if (strcasecmp(settingName, "eq") == 0) {
    EQ = atoi(settingValue);
    if (EQ < 0) {
      EQ = 0;
    } else if (EQ > 1) {
      EQ = 1;
    }
  } else if (strcasecmp(settingName, "eq_bands") == 0) {
    // clear bands and prep for setting
    for (int i = 0; i < EQ_BANDS_SIZE; i++) {
      EQ_BANDS[i] = 0;
    }
    char *band, *ptr;
    band = strtok_r(settingValue, ",", &ptr);
    int i = 0;
    while (band && i < EQ_BANDS_SIZE) {
      EQ_BANDS[i] = atof(band);
      i++;
      band = strtok_r(NULL, ",", &ptr);
    }
  } else if (strcasecmp(settingName, "bitcrushers") == 0) {
    char *token, *ptr;
    token = strtok_r(settingValue, ",", &ptr);
    int i = 0;
    while (token && i < 5) {
      BITCRUSHER[i] = atoi(token);
      i++;
      token = strtok_r(NULL, ",", &ptr);
    }
  } else if (strcasecmp(settingName, "button_pin") == 0) {
    BUTTON_PIN = atoi(settingValue);
  } else if (strcasecmp(settingName, "effects_dir") == 0) {
    memset(EFFECTS_DIR, 0, sizeof(EFFECTS_DIR));
    strcpy(EFFECTS_DIR, settingValue);
    fixPath(EFFECTS_DIR);
    loadSoundEffects();
  } else if (strcasecmp(settingName, "sounds_dir") == 0) {
    memset(SOUNDS_DIR, 0, sizeof(SOUNDS_DIR));
    strcpy(SOUNDS_DIR, settingValue);
    fixPath(SOUNDS_DIR);
  } else if (strcasecmp(settingName, "loop_dir") == 0) {
    memset(LOOP_DIR, 0, sizeof(LOOP_DIR));
    strcpy(LOOP_DIR, settingValue);
    fixPath(LOOP_DIR);
  }
}

/**
 * Create JSON string of settings 
 * (used for app)
 */
char *settingsToJson(char result[]) 
{

  const char str_template[12] = "\"%s\":\"%s\"";
  const char num_template[12] = "\"%s\":%s";
  char buf[20];
  char tmp[100];

  sprintf(tmp, str_template, "name", PROFILE_NAME);
  strcpy(result, tmp);
  strcat(result, ",");

  dtostrf(MASTER_VOLUME, 0, 4, buf);
  sprintf(tmp, num_template, "volume", buf);
  strcat(result, tmp);
  strcat(result, ",");

  sprintf(buf, "%d", MIC_GAIN);
  sprintf(tmp, num_template, "mic_gain", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  sprintf(buf, "%d", LINEIN);
  sprintf(tmp, num_template, "linein", buf);
  strcat(result, tmp);
  strcat(result, ",");

  sprintf(buf, "%d", LINEOUT);
  sprintf(tmp, num_template, "lineout", buf);
  strcat(result, tmp);
  strcat(result, ",");

  sprintf(tmp, str_template, "startup", STARTUP_WAV);
  strcat(result, tmp);
  strcat(result, ",");

  sprintf(tmp, str_template, "loop", LOOP_WAV);
  strcat(result, tmp);
  strcat(result, ",");

  dtostrf(LOOP_GAIN, 0, 4, buf);
  sprintf(tmp, num_template, "loop_gain", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  sprintf(buf, "%d", HIPASS);
  sprintf(tmp, num_template, "high_pass", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  dtostrf(VOICE_GAIN, 0, 4, buf);
  sprintf(tmp, num_template, "voice_gain", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  dtostrf(VOICE_START, 0, 4, buf);
  sprintf(tmp, num_template, "voice_start", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  dtostrf(VOICE_STOP, 0, 4, buf);
  sprintf(tmp, num_template, "voice_stop", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  sprintf(buf, "%d", SILENCE_TIME);
  sprintf(tmp, num_template, "silence_time", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  
  sprintf(buf, "%d", BUTTON_PIN);
  sprintf(tmp, num_template, "button_pin", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  sprintf(tmp, str_template, "button_click", BUTTON_WAV);
  strcat(result, tmp);
  strcat(result, ",");
  
  sprintf(buf, "%d", AUDIO_INPUT);
  sprintf(tmp, num_template, "input", buf);
  strcat(result, tmp);
  strcat(result, ",");
  
  dtostrf(EFFECTS_GAIN, 0, 4, buf);
  sprintf(tmp, num_template, "effects_gain", buf);
  strcat(result, tmp);
  strcat(result, ",");

  sprintf(buf, "%d", EQ);
  sprintf(tmp, num_template, "eq", buf);
  strcat(result, tmp);
  strcat(result, ",");

  char buffer[SETTING_ENTRY_MAX];
  char *bands = arrayToString(buffer, EQ_BANDS, EQ_BANDS_SIZE);
  sprintf(tmp, str_template, "eq_bands", bands);
  strcat(result, tmp);
  strcat(result, ",");
  memset(buffer, 0, sizeof(buffer));
  
  char *bitcrushers = arrayToString(buffer, BITCRUSHER, BITCRUSHER_SIZE);
  sprintf(tmp, str_template, "bitcrushers", bitcrushers);
  strcat(result, tmp);
  strcat(result, ",");
  memset(buffer, 0, sizeof(buffer));
  
  dtostrf(NOISE_GAIN, 0, 4, buf);
  sprintf(tmp, num_template, "noise_gain", buf);
  strcat(result, tmp);
  strcat(result, ",");

  sprintf(tmp, str_template, "effects_dir", EFFECTS_DIR);
  strcat(result, tmp);
  strcat(result, ",");

  sprintf(tmp, str_template, "sounds_dir", SOUNDS_DIR);
  strcat(result, tmp);
  strcat(result, ",");

  sprintf(tmp, str_template, "loop_dir", LOOP_DIR);
  strcat(result, tmp);

  return result;
  
}

/***
 * Converts all in-memory settings to string
 */
char *settingsToString(char result[]) 
{
  char buf[SETTING_ENTRY_MAX];

  strcpy(result, "[name=");
  strcat(result, PROFILE_NAME);
  strcat(result, "]\n");

  if (MASTER_VOLUME > 0) {
    strcat(result, "[volume=");
    dtostrf(MASTER_VOLUME, 0, 4, buf);
    strcat(result, buf);
    strcat(result, "]\n");
    memset(buf, 0, sizeof(buf));
  }

  strcat(result, "[linein=");
  sprintf(buf, "%d", LINEIN);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[lineout=");
  sprintf(buf, "%d", LINEOUT);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[startup=");
  strcat(result, STARTUP_WAV);
  strcat(result, "]\n");

  strcat(result, "[loop=");
  strcat(result, LOOP_WAV);
  strcat(result, "]\n");

  strcat(result, "[loop_gain=");
  dtostrf(LOOP_GAIN, 0, 4, buf);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));  

  strcat(result, "[high_pass=");
  sprintf(buf, "%d", HIPASS);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[voice_gain=");
  dtostrf(VOICE_GAIN, 0, 4, buf);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[voice_start=");
  dtostrf(VOICE_START, 0, 4, buf);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[voice_stop=");
  dtostrf(VOICE_STOP, 0, 4, buf);
  strcat(result, buf);
  strcat(result, "]\n");

  memset(buf, 0, sizeof(buf));
  strcat(result, "[silence_time=");
  sprintf(buf, "%d", SILENCE_TIME);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[button_pin=");
  sprintf(buf, "%d", BUTTON_PIN);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[button_click=");
  strcat(result, BUTTON_WAV);
  strcat(result, "]\n");

  strcat(result, "[input=");
  sprintf(buf, "%d", AUDIO_INPUT);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[mic_gain=");
  sprintf(buf, "%d", MIC_GAIN);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[effects_gain=");
  dtostrf(EFFECTS_GAIN, 0, 4, buf);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[eq=");
  sprintf(buf, "%d", EQ);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[eq_bands=");
  char *bands = arrayToString(buf, EQ_BANDS, EQ_BANDS_SIZE);
  strcat(result, bands);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[bitcrushers=");
  char *bitcrushers = arrayToString(buf, BITCRUSHER, BITCRUSHER_SIZE); 
  strcat(result, bitcrushers);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[noise_gain=");
  dtostrf(NOISE_GAIN, 0, 4, buf);
  strcat(result, buf);
  strcat(result, "]\n");
  memset(buf, 0, sizeof(buf));

  strcat(result, "[effects_dir=");
  strcat(result, EFFECTS_DIR);
  strcat(result, "]\n");

  strcat(result, "[sounds_dir=");
  strcat(result, SOUNDS_DIR);
  strcat(result, "]\n");

  strcat(result, "[loop_dir=");
  strcat(result, LOOP_DIR);
  strcat(result, "]\n");

  return result;
  
}

/**
 * Sends config via Bluetooth Serial.  Used for TKTalkie App
 */
void sendConfig() 
{

  debug(F("Sending config\n"));

  btprint(F("{\"cmd\":\"config\", \"data\": { \"ver\":\"%s\","), VERSION);

  // get sound files
  char files[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
  
  int count = listFiles(SOUNDS_DIR, files, MAX_FILE_COUNT, SOUND_EXT, false, false);

  // Add sound files
  char buffer[SETTING_ENTRY_MAX];
  char *sounds = arrayToStringJson(buffer, files, count);
  btprint(F("\"sounds\":%s,"), sounds);
  memset(buffer, 0, sizeof(buffer));

  // Clear array
  memset(files, 0, sizeof(files));

  // Add effects 
  char *effects = arrayToStringJson(buffer, SOUND_EFFECTS, SOUND_EFFECTS_COUNT);
  btprint(F("\"effects\":%s,"), effects);
  memset(buffer, 0, sizeof(buffer));

  // get loop files 
  count = listFiles(LOOP_DIR, files, MAX_FILE_COUNT, SOUND_EXT, false, false);
  
  // Add loops 
  char *loops = arrayToStringJson(buffer, files, count);
  btprint(F("\"loops\":%s,"), loops);
  memset(buffer, 0, sizeof(buffer));

  // Clear array
  memset(files, 0, sizeof(files));

  char *profile = getSettingValue(buffer, "profile");
  btprint(F("\"default\":\"%s\","), profile);
  memset(buffer, 0, sizeof(buffer));
  
  btprint(F("\"current\": {\"name\":\"%s\",\"desc\":\"%s\"},"), PROFILE_FILE, PROFILE_NAME);

  // This is already formatted correctly
  char *json = settingsToJson(buffer);
  btprint(F("%s,"), json);
  memset(buffer, 0, sizeof(buffer));

  btprint(F("\"profiles\":["));
  
  // get config profile files 
  count = listFiles(PROFILES_DIR, files, MAX_FILE_COUNT, FILE_EXT, false, false);
  
  for (int i = 0; i < count; i++) {
     char entries[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
     char filename[SETTING_ENTRY_MAX];
     strcpy(filename, PROFILES_DIR);
     strcat(filename, files[i]);
     int total = loadSettingsFile(filename, entries, MAX_FILE_COUNT);
     for (int x = 0; x < total; x++) {
        char *key, *value, *ptr;
        char entry[SETTING_ENTRY_MAX];
        strcpy(entry, entries[x]);
        key = strtok_r(entry, "=", &ptr);
        value = strtok_r(NULL, "=", &ptr);
        if (strcasecmp(key, "name") == 0) {
           btprint(F("{\"name\":\"%s\",\"desc\":\"%s\"}"), files[i], value);
           if (i < count-1) {
             btprint(F(","));
           }
           break;
        }
     }
  }
  
  btprint(F("],\"mute\":%s,"), MUTED == true ? "1" : "0");

  // end
  btprint(F("\"bg\":%s}}\n"), loopPlayer.isPlaying() ? "1" : "0");

}

/**
 * Save startup settings
 */
boolean saveSettings() {
  debug(F("Saving config data:\n"));
  File srcFile = openFile(SETTINGS_FILE, FILE_WRITE);
  if (srcFile) {
    for (int i = 0; i < STARTUP_SETTINGS_COUNT; i++) {
      debug(F("%d: %s\n"), i, STARTUP_SETTINGS[i]);
      srcFile.print("[");
      srcFile.print(STARTUP_SETTINGS[i]);
      srcFile.print("]\n");      
    }
    srcFile.close();
    debug(F("Startup Settings Updated\n"));
    return true;
  } else {
    debug(F("**ERROR** Updating Startup Settings!\n"));
    beep(4);
    return false;
  }
}

/**
 * Backup settings to specified file
 */
boolean saveSettingsFile(const char *src) 
{
  char filename[SETTING_ENTRY_MAX], backup[SETTING_ENTRY_MAX];
  char bak[5] = ".BAK";
  boolean result = false;
  if (strcasecmp(src, "") == 0) {
    strcpy(filename, PROFILE_FILE);
  } else {
    strcpy(filename, src);
  }
  // get last index of "." for file extension
  char *ret = strstr(filename, ".");
  if (ret == NULL) {
    strcat(filename, FILE_EXT);
  }
  char *ptr, *fname;
  fname = strtok_r(filename, ".", &ptr);
  strcpy(backup, fname);
  strcat(backup, bak);
  strcat(filename, FILE_EXT);
  char bakFileName[25];
  char srcFileName[25];
  strcpy(bakFileName, PROFILES_DIR);
  strcat(bakFileName, backup);
  debug(F("Backup File: %s%s"), PROFILES_DIR, bakFileName);
  strcpy(srcFileName, PROFILES_DIR);
  strcat(srcFileName, filename);
  File bakFile = openFile(bakFileName, FILE_WRITE);
  File srcFile = openFile(srcFileName, FILE_READ);
  debug(F("BACKUP FILE: %s\n"), bakFileName);
  if (bakFile && srcFile) {
    char c;
    while (srcFile.available()) {
      c = srcFile.read();
      bakFile.write(c);
    }
    bakFile.close();
    srcFile.close();
  } else {
    debug(F("**ERROR** creating backup file!\n"));
    if (srcFile) {
      srcFile.close();
    }
    if (bakFile) {
      bakFile.close();
    }
  }
  // add extension back to filename
  debug(F("Save to: %s\n"), srcFileName);
  File newFile = openFile(srcFileName, FILE_WRITE);
  if (newFile) {
    char buffer[1024];
    char *p = settingsToString(buffer);
    newFile.println(p);
    newFile.close();
    result = true;
    Serial.print(F("Settings saved to "));
    Serial.println(srcFileName);
  } else {
    Serial.print(F("**ERROR** saving to: "));
    Serial.println(srcFileName);
  }
  return result;
}

/**
 * Set a startup setting value
 */
void setSettingValue(const char *key, const char *newValue)
{
   int index = -1;
   char newKey[SETTING_ENTRY_MAX];
   for (int i = 0; i < STARTUP_SETTINGS_COUNT; i++) {
    char data[SETTING_ENTRY_MAX];
    strcpy(data, STARTUP_SETTINGS[i]);
    if (strcasecmp(data, "") != 0) {
      char *settingKey, *ptr;
      settingKey = strtok_r(data, "=", &ptr);
      if (strcasecmp(key, settingKey) == 0) {
        index = i;
        break;
      }
    } else {
      // first blank space
      index = i;
      break;
    }
  } 

  // This shouldn't happen, but just in case ;)
  if (index > STARTUP_SETTINGS_COUNT - 1) {
    Serial.println("Invalid setting index!");
    return;
  }
  
  char buf[SETTING_ENTRY_MAX];
  strcpy(newKey, key);
  strcat(newKey, "=");
  strcpy(buf, newKey);
  strcat(buf, newValue);
  memset(STARTUP_SETTINGS[index], 0, SETTING_ENTRY_MAX);
  strcpy(STARTUP_SETTINGS[index], buf);  
  
}

/**
 * Retrieve a startup setting value
 */
char *getSettingValue(char result[], const char *key) 
{
    debug(F("Get setting: %s\n"), key);
    
    for (int i = 0; i < STARTUP_SETTINGS_COUNT; i++) {
      char setting[SETTING_ENTRY_MAX] = "";
      strcpy(setting, STARTUP_SETTINGS[i]);
      if (strcasecmp(setting, "") != 0) {
        char *name, *value, *ptr;
        name = strtok_r(setting, "=", &ptr);
        value = strtok_r(NULL, "=", &ptr);
        debug(F("Check setting %s = %s\n"), name, value);
        if (strcasecmp(name, key) == 0) {
          result = value;
          break;
        }
      }
    }
    debug(F("Return value %s\n"), result);

    return result;
}


/**
 * Set the specified file as the default profile that 
 * is loaded with TKTalkie starts
 */
boolean setDefaultProfile(const char *filename) 
{
    char fname[SETTING_ENTRY_MAX];
    char *ret = strstr(filename, ".");
    if (ret == NULL) {
      strcpy(fname, filename);
      strcat(fname, FILE_EXT);
    } else {
      strcpy(fname, filename);
    }
    debug(F("Setting default profile to %s\n"), fname);
    char profiles[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
    int total = listFiles(PROFILES_DIR, profiles, MAX_FILE_COUNT, FILE_EXT, false, false);
    boolean result = false;
    boolean found = false;
    for (int i = 0; i < total; i++) {
      if (strcasecmp(profiles[i], fname) == 0) {
        setSettingValue("profile", fname);
        found = true;
        break;
      }
    }

    // save results to file if entry was not found
    if (found == true) {
      result = saveSettings();
    } else {
      debug(F("Filename was not an existing profile\n"));
    }  

    if (result == true) {
      debug(F("Default profile set\n"));
    } else {
      debug(F("**ERROR** setting default profile\n"));
    }
  
    return result;
}

/**
 * Remove a profile from the list and delete the file
 */
boolean deleteProfile(const char *filename) 
{
  
  boolean result = false;

  // can't delete current profile
  if (strcasecmp(filename, PROFILE_FILE) == 0){
    debug(F("Cannot delete current profile\n"));
    result = false;
  } else {
    result = deleteFile(filename);
  }

  // if the profile filename was the default profile, 
  // set the default profile to the currently loaded profile
  char buffer[SETTING_ENTRY_MAX];
  char *profile = getSettingValue(buffer, "profile");
  if (strcasecmp(filename, profile) == 0) {
    debug(F("Profile was default -> Setting default profile to current profile\n"));
    setSettingValue("profile", PROFILE_FILE);
    saveSettings();
  }
  return result;
  
}

/***
 * Read settings from specified file
 */
int loadSettingsFile(const char *filename, char results[][SETTING_ENTRY_MAX], int max) 
{
  debug(F("Load Settings File %s\n"), filename);
  char character;
  char settingName[SETTING_ENTRY_MAX] = "";
  char settingValue[SETTING_ENTRY_MAX] = "";
  File myFile = openFile(filename, FILE_READ);
  int index = 0;
  int c = 0;
  if (myFile) {
    while (myFile.available() && index < max) {
      character = myFile.read();
      while ((myFile.available()) && (character != '[')) {
        character = myFile.read();
      }
      character = myFile.read();
      settingName[c] = character;
      c += 1 ;
      while ((myFile.available()) && (character != '='))  {
        character = myFile.read();
        if (character != '=') {
          settingName[c] = character;
          c += 1;
        }
      }
      character = myFile.read();
      c = 0;
      while ((myFile.available()) && (character != ']'))  {
        settingValue[c] = character;
        c++;
        character = myFile.read();
      }
      if  (character == ']')  {
        c = 0;
        if (index <= max && strcasecmp(settingName, "") != 0) {
          char buf[SETTING_ENTRY_MAX] = "";
          strcpy(buf, settingName);
          strcat(buf, "=");
          strcat(buf, settingValue);
          strcpy(results[index], buf);
          index++;
        } else {
          break;
        }
      }
      memset(settingName, 0, sizeof(settingName));
      memset(settingValue, 0, sizeof(settingValue));
    }
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    debug(F("**ERROR** opening settings file %s\n"), filename);
    index = 0;
    beep(3);
  }
  return index;
}

/**
 * Process a list of settings values
 */
void processSettings(char settings[][SETTING_ENTRY_MAX], const int max)
{
  for (int i = 0; i < max; i++) {
    char entry[SETTING_ENTRY_MAX];
    strcpy(entry, settings[i]);
    char *key, *value, *ptr;
    key = strtok_r(entry, "=", &ptr);
    value = strtok_r(NULL, "=", &ptr);
    parseSetting(key, value);
  }
}

/**
 * Load specified settings file
 */
int loadSettings(const char *filename) 
{
  char settings[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
  int total = loadSettingsFile(filename, settings, MAX_FILE_COUNT);
  Serial.println("AFTER LOAD SETTINGS, CALLING PROCESS SETTINGS");
  processSettings(settings, total);
  Serial.print("RETURNING TOTAL: ");
  Serial.println(total);
  return total;
}

/***
 * Apply settings
 */
void applySettings() 
{
  // Turn on the 5-band graphic equalizer (there is also a 7-band parametric...see the Teensy docs)
  if (EQ == 0) {
    audioShield.eqSelect(FLAT_FREQUENCY);
  } else {
    audioShield.eqSelect(GRAPHIC_EQUALIZER);
    // Bands (from left to right) are: Low, Low-Mid, Mid, High-Mid, High.
    // Valid values are -1 (-11.75dB) to 1 (+12dB)
    // The settings below pull down the lows and highs and push up the mids for 
    // more of a "tin-can" sound.
    audioShield.eqBands(EQ_BANDS[0], EQ_BANDS[1], EQ_BANDS[2], EQ_BANDS[3], EQ_BANDS[4]);
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
  // BLE connect sound
  effectsMixer.gain(2, EFFECTS_GAIN);
  // Feed from loop mixer
  effectsMixer.gain(3, 1);
  // chatter loop from SD card
  loopMixer.gain(0, LOOP_GAIN);
  loopMixer.gain(1, LOOP_GAIN);
  loopMixer.gain(3, EFFECTS_GAIN);
  audioShield.volume(readVolume());
  audioShield.lineOutLevel(LINEOUT);
  if (HIPASS == 0) {
    audioShield.adcHighPassFilterDisable();
  } else {
    audioShield.adcHighPassFilterEnable();
  }
  voiceOff();
}

/**
 * Read the SETTINGS.TXT file
 */
void startup() 
{

  // make sure we have a profile to load
  memset(PROFILE_FILE, 0, sizeof(PROFILE_FILE));

  // Load all entries in PROFILES.TXT file.
  // The file to load will be in the format default=filename.
  // All other entries are in the format profile=filename.
  int total = loadSettingsFile(SETTINGS_FILE, STARTUP_SETTINGS, STARTUP_SETTINGS_COUNT);

  char buffer[SETTING_ENTRY_MAX];
  char *buf;
  
  if (total > 0) {

    buf = getSettingValue(buffer, "debug");
    DEBUG = (strcasecmp(buf, "1") == 0) ? true : false;
    debug(F("Got startup value DEBUG: %s\n"), (DEBUG == true ? "true" : "false"));

    buf[0] = '\0';
    memset(buffer, 0, sizeof(buffer));
    
    buf = getSettingValue(buffer, "profile");
    strcpy(PROFILE_FILE, buf);
    debug(F("Got startup value PROFILE: %s\n"), PROFILE_FILE);

    buf[0] = '\0';
    memset(buffer, 0, sizeof(buffer));
    
    buf = getSettingValue(buffer, "echo");
    ECHO = (strcasecmp(buf, "1") == 0) ? true : false;
    debug(F("Got startup  value ECHO: %s\n"), (ECHO == true ? "true" : "false"));
    
  }

  Serial.println(F("\n----------------------------------------------"));
  Serial.println(F("TKTalkie v3.0"));
  Serial.println(F("(c) 2017 TK81113/Because...Interwebs!\nwww.TKTalkie.com"));
  Serial.print(F("Debugging is "));
  Serial.println(DEBUG == true ? "ON" : "OFF");
  if (DEBUG == false) {
    Serial.println(F("Type debug=1 [ENTER] to enable debug messages"));
  } else {
    Serial.println(F("Type debug=0 [ENTER] to disable debug messages"));
  }
  Serial.println(F("----------------------------------------------\n"));
  
  if (strcasecmp(PROFILE_FILE, "") == 0) {
    // No profile specified, try to find one and load it
    char files[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
    total = listFiles(PROFILES_DIR, files, MAX_FILE_COUNT, FILE_EXT, false, false);
    if (total > 0) {
      memset(PROFILE_FILE, 0, sizeof(PROFILE_FILE));
      strcpy(PROFILE_FILE, files[0]);
      // If no startup profiles were found, set default and save
      if (total < 1) {
        setSettingValue("profile", PROFILE_FILE);
        saveSettings();
      }
    } else {
      debug(F("NO PROFILES LISTED\n"));
    }
  }

  if (strcasecmp(PROFILE_FILE, "") == 0) {
    debug(F("NO PROFILE FILE FOUND!\n"));
    return;
  }

  debug(F("PROFILE: %s\n"), PROFILE_FILE);

  buf[0] = '\0';
  memset(buffer, 0, sizeof(buffer));
  buf = getSettingValue(buffer, "access_code");
  debug(F("Read access code %s\n"), buf);
  if (strcasecmp(buf, "") != 0) {
    memset(ACCESS_CODE, 0, sizeof(ACCESS_CODE));
    strcpy(ACCESS_CODE, buf);
  }
  
  char profile_settings[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
  
  // Load settings from specified file
  char path[100];
  strcpy(path, PROFILES_DIR);
  strcat(path, PROFILE_FILE);
  total = loadSettingsFile(path, profile_settings, MAX_FILE_COUNT);

  // Parse all of the settings
  processSettings(profile_settings, total);

  // apply the settings so we can do stuff
  applySettings();

  // set the volume, either by config or volume pot
  readVolume();

  // turn on outputs
  audioShield.unmuteLineout();
  audioShield.unmuteHeadphone();

  // play startup sound
  long l = playSound(STARTUP_WAV);

  // add a smidge of delay ;)
  delay(l+100); 

  // play background loop
  playLoop();

  STATE = STATE_RUNNING;

}

/***
 * Initial setup...runs only once when the board is turned on.
 */
void setup() 
{
  STATE = STATE_BOOTING;
  
  // You really only need the Serial connection 
  // for output while you are developing, so you 
  // can uncomment this and use Serial.println()
  // to write messages to the console.
  Serial.begin(57600);
  
  Serial1.begin(9600);
  
  delay(500);

  // Always allocate memory for the audio shield!
  AudioMemory(16);
  
  // turn on audio shield
  audioShield.enable();
  
  // disable volume and outputs during setup to avoid pops
  audioShield.muteLineout();
  audioShield.muteHeadphone();
  audioShield.volume(0);

  // turn on post processing
  audioShield.audioPostProcessorEnable();

  // Initialize sound generator for warning tones when we switch modes from PTT back to Voice Activated
  waveform1.begin(WAVEFORM_SINE);
  
  // Check SD card
  SPI.setMOSI(7);  // Set to 7 for Teensy
  SPI.setSCK(14);  // Set to 14 for Teensy
  // CS Pin is 10..do not change for Teensy!
  if (!(SD.begin(10))) {
     Serial.println("Unable to access the SD card");
     beep(2);
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

  // load startup settings
  startup();
  
}

/***
 * Main program loop
 */

void loop() 
{

  switch (STATE) {
    case STATE_NONE:
    case STATE_BOOTING:
      // do nothing
      break;
    case STATE_RUNNING:
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
  if (STATE == STATE_RUNNING) {

    if (loopLength && loopMillis > loopLength) {
        playLoop();
    }

    if (Serial.available() > 0) { 
      Serial.readBytesUntil('\n', received, MAX_DATA_SIZE);
      char *key, *val, *buf;
      key = strtok_r(received, "=", &buf);
      val = strtok_r(NULL, "=", &buf);
      strcpy(cmd_key, key);
      strcpy(cmd_val, val);
    } else if (Serial1.available() > 0) {
      char *key, *val, *buf, *buf2, *uid;      
      Serial1.readBytesUntil('\n', received, MAX_DATA_SIZE);
      debug(F("RX: %s\n"), received);
      key = strtok_r(received, "=", &buf);
      val = strtok_r(NULL, "=", &buf);
      uid = strtok_r(val, "|", &buf2);
      val = strtok_r(NULL, "|", &buf2);
      strcpy(cmd_key, key);
      strcpy(cmd_val, val);
      debug(F("BLE Cmd: %s Value: %s Uid: %s\n"), cmd_key, cmd_val, uid);
      // validate data received from mobile device!
      if (strcasecmp(cmd_key, "connect") == 0) {
          debug(F("Received access code %s\n"), cmd_val);
          if (strcmp(ACCESS_CODE, cmd_val) == 0) {
            connectSound();
            BT_CONNECTED = true;
            memset(DEVICE_ID, 0, sizeof(DEVICE_ID));
            strcpy(DEVICE_ID, uid);
            debug(F("DEVICE ID %s...Send Access OK\n"), DEVICE_ID);
            sendToApp("access", "1");
          } else {
            BT_CONNECTED = true;
            sendToApp("access", "0");
            BT_CONNECTED = false;
          }
          strcpy(cmd_key, "");
      } else {
          if (strcmp(DEVICE_ID, uid) == 0) {
            // Process remote commands
            if (strcasecmp(cmd_key, "disconnect") == 0) {
                BT_CONNECTED = false;
                disconnectSound();
                memset(DEVICE_ID, 0, sizeof(DEVICE_ID));
                memset(cmd_key, 0, sizeof(cmd_key));
                memset(cmd_val, 0, sizeof(cmd_val));
            } else if (strcasecmp(cmd_key, "config") == 0) {
                sendConfig();
                memset(cmd_key, 0, sizeof(cmd_key));
                memset(cmd_val, 0, sizeof(cmd_val));
            }
          } else {
            // The UUID does not match the one connected, 
            // so clear the command.            
            memset(cmd_key, 0, sizeof(cmd_key));
            memset(cmd_val, 0, sizeof(cmd_val));
          }
      }
    }
    
    if (strcasecmp(cmd_key, "") != 0) {
      Serial.print(">");
      Serial.print(cmd_key);
      if (strcasecmp(cmd_val, "") != 0) {
        Serial.print("=");
        Serial.print(cmd_val);
      }
      Serial.println("<");
      // Check if there is a parameter and process 
      // commands with values first
      if (strcasecmp(cmd_key, "config") == 0) {
        for (int i = 0; i < STARTUP_SETTINGS_COUNT; i++) {
            Serial.println(STARTUP_SETTINGS[i]);
          }
      } else if (strcasecmp(cmd_key, "save") == 0) {
        if (strcasecmp(cmd_val, "") != 0) {
            char *ptr, *pfile, *pname;
            pfile = strtok_r(cmd_val, ";", &ptr);
            if (strcasecmp(pfile, "") != 0) {
              memset(PROFILE_FILE, 0, sizeof(PROFILE_FILE));
              strcpy(PROFILE_FILE, pfile);
            }
            pname = strtok_r(NULL, ";", &ptr);
            if (strcasecmp(pname, "") != 0) {
              memset(PROFILE_NAME, 0, sizeof(PROFILE_NAME));
              strcpy(PROFILE_NAME, pname);
            }
         }
         char *ptr = strstr(PROFILE_FILE, ".");
         if (ptr == NULL) {
          strcat(PROFILE_FILE, FILE_EXT);
         }
         debug(F("Save settings file %s with description %s\n"), PROFILE_FILE, PROFILE_NAME);
         if (saveSettingsFile(PROFILE_FILE) == true) {
          sendToApp("save", "1");
         } else {
          sendToApp("save", "0");
         }
      } else if (strcasecmp(cmd_key, "access_code") == 0) {
           if (strcasecmp(cmd_val, "") != 0) {
              memset(ACCESS_CODE, 0, sizeof(ACCESS_CODE));
              strcpy(ACCESS_CODE, cmd_val);
              setSettingValue("access_code", ACCESS_CODE);
              saveSettings();
          }
      } else if (strcasecmp(cmd_key, "debug") == 0) {
           if (strcasecmp(cmd_val, "") == 0) {
              Serial.print("DEBUG=");
              Serial.println(DEBUG == true ? "1" : "0");
           } else {
              int i = atoi(cmd_val);
              char val[2] = "0";
              if (i == 0) {
                DEBUG = false;
              } else {
                strcpy(val, "1");
                DEBUG = true;
              }
              setSettingValue("debug", val);
              saveSettings();
          }
      } else if (strcasecmp(cmd_key, "echo") == 0) {
           if (strcasecmp(cmd_val, "") == 0) {
              Serial.print("ECHO=");
              Serial.println(ECHO == true ? "1" : "0");
           } else {
              int i = atoi(cmd_val);
              char val[2] = "0";
              if (i == 0) {
                ECHO = false;
              } else {
                strcpy(val, "1");
                ECHO = true;
              }
              setSettingValue("echo", val);
              saveSettings();
          }
      } else if (strcasecmp(cmd_key, "default") == 0) {
          if (strcasecmp(cmd_val, "") != 0) {
            char ret[SETTING_ENTRY_MAX];
            if (setDefaultProfile(cmd_val)) {
              strcpy(ret, "1;");
            } else {
              strcpy(ret, "0;");
            }
            strcat(ret, cmd_val);
            sendToApp("default", ret);
          } else {
            sendToApp("default", "0");
          }
      } else if (strcasecmp(cmd_key, "delete") == 0) {
          char *ret = strstr(cmd_val, ".");
          if (ret == NULL) {
            strcat(cmd_val, FILE_EXT);
          }
          if (deleteProfile(cmd_val)) {
            strcpy(ret, "1;");
            strcat(ret, cmd_val);
          } else {
            strcpy(ret, "0;Could not remove profile");
          }
          sendToApp("delete", ret);
      } else if (strcasecmp(cmd_key, "load") == 0) {
          if (strcasecmp(cmd_val, "") != 0) {
            memset(PROFILE_FILE, 0, sizeof(PROFILE_FILE));
            strcpy(PROFILE_FILE, cmd_val);
          } 
          char *ptr = strstr(PROFILE_FILE, ".");
          if (ptr == NULL) {
            strcat(PROFILE_FILE, FILE_EXT);
          }
          char buf[100];
          strcpy(buf, PROFILES_DIR);
          strcat(buf, PROFILE_FILE);
          Serial.print("LOADING FILE: ");
          Serial.println(buf);
          loadSettings(buf);
          Serial.println("CALLING APPLY SETTINGS");
          applySettings();
          Serial.println("CALLING PLAY SOUND");
          playSound(STARTUP_WAV);
          Serial.println("CALLING PLAY LOOP");
          playLoop();
          // send to remote if connected
          Serial.println("CALLING SEND CONFIG");
          sendConfig();
      } else if (strcasecmp(cmd_key, "play") == 0) {
          //playSoundFile(EFFECTS_PLAYER, cmd_val);
          effectsPlayer.play(cmd_val);
      } else if (strcasecmp(cmd_key, "play_effect") == 0) {
          playEffect(cmd_val);
      } else if (strcasecmp(cmd_key, "play_sound") == 0) {
          playSound(cmd_val);
      } else if (strcasecmp(cmd_key, "play_loop") == 0) {
          memset(LOOP_WAV, 0, sizeof(LOOP_WAV));
          strcpy(LOOP_WAV, cmd_val);
          playLoop();
      } else if (strcasecmp(cmd_key, "stop_loop") == 0) {
         loopPlayer.stop();
      } else if (strcasecmp(cmd_key, "beep") == 0) {
          int i = atoi(cmd_val);
          if (i < 1) {
            i = 1;
          }
          beep(i);
      } else if (strcasecmp(cmd_key, "mute") == 0) {
         audioShield.muteHeadphone();
         audioShield.muteLineout();
         MUTED = true;
      } else if (strcasecmp(cmd_key, "unmute") == 0) {
         audioShield.unmuteHeadphone();
         audioShield.unmuteLineout();
         MUTED = false;
      } else if (strcasecmp(cmd_key, "backup") == 0) {
         saveSettingsFile(BACKUP_FILE); 
      } else if (strcasecmp(cmd_key, "restore") == 0) {
         loadSettings(BACKUP_FILE);    
         applySettings();
      } else if (strcasecmp(cmd_key, "settings") == 0) {
          Serial.println(F(""));
          Serial.println(PROFILE_FILE);
          Serial.println(F("--------------------------------------------------------------------------------"));
          char buffer[1024];
          char *p = settingsToString(buffer);
          Serial.println(p);
          Serial.println(F("--------------------------------------------------------------------------------"));
          Serial.println(F(""));
      } else if (strcasecmp(cmd_key, "files") == 0) {
          char temp[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
          listFiles("/", temp, MAX_FILE_COUNT, "", true, true);
      } else if (strcasecmp(cmd_key, "show") == 0) {
          showFile(cmd_val);
      } else if (strcasecmp(cmd_key, "sounds") == 0) {
          char temp[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
          listFiles(SOUNDS_DIR, temp, MAX_FILE_COUNT, SOUND_EXT, false, true);
      } else if (strcasecmp(cmd_key, "effects") == 0) {
          char temp[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
          listFiles(EFFECTS_DIR, temp, MAX_FILE_COUNT, SOUND_EXT, false, true);
      } else if (strcasecmp(cmd_key, "loops") == 0) {
          char temp[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
          listFiles(LOOP_DIR, temp, MAX_FILE_COUNT, SOUND_EXT, false, true);
      } else if (strcasecmp(cmd_key, "profiles") == 0) {
          char temp[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
          int count = listFiles(PROFILES_DIR, temp, MAX_FILE_COUNT, FILE_EXT, false, false);
          char buffer[1024];
          char *def = getSettingValue(buffer, "profile");
          for (int i = 0; i < count; i++) {
            Serial.print(temp[i]);
            if (strcasecmp(temp[i], PROFILE_FILE) == 0) {
              Serial.print(" (Loaded)");
            }
            if (strcasecmp(temp[i], def) == 0) {
              Serial.print(" (Default)");
            }
            Serial.println("");
          }
      } else if (strcasecmp(cmd_key, "ls") == 0) {
          char paths[MAX_FILE_COUNT][SETTING_ENTRY_MAX];
          int count = listDirectories("/", paths);
          char buffer[1024];
          char *dirs = arrayToString(buffer, paths, count);
          sendToApp(cmd_key, dirs);
      } else if (strcasecmp(cmd_key, "help") == 0) {
          showFile("HELP.TXT");
      } else if (strcasecmp(cmd_key, "calibrate") == 0) {
          calibrate();
      } else if (strcasecmp(cmd_key, "reset") == 0) {
        softreset();
      } else { 
        parseSetting(cmd_key, cmd_val);
        applySettings();
        if (strcasecmp(cmd_key, "loop") == 0) {
          playLoop();
        }  
      }
      Serial.println("");
      memset(cmd_key, 0, sizeof(cmd_key));
      memset(cmd_val, 0, sizeof(cmd_val));
    }
  
    if (BUTTON_PIN && button_initialized == false) {
      button_initialized = checkButton();
      if (button_initialized) {
        // turn voice on with background noise
        voiceOn();
      }
    } else {
      PTT.update();
    }

    
    if (BUTTON_PIN && button_initialized) {
  
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
        connectSound();
        return;
      }
  
      // Button press
      
      if (PTT.fell()) {
        playEffect(BUTTON_WAV);
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
  
        // Check if we have audio
        if (rms1.available()) {
          
          // get the input amplitude...will be between 0.0 (no sound) and 1.0
          float val = rms1.read();
    
          // Uncomment this to see what your constant signal is
          //Serial.println(val);
          
          // Set a minimum and maximum level to use or you will trigger it accidentally 
          // by breathing, etc.  Adjust this level as necessary!
          if (val >= VOICE_START) {
  
             debug(F("Voice start: %4f\n"), val);
             
            // If user is not currently speaking, then they just started talking :)
            if (speaking == false) {
  
              loopOff();
              voiceOn();
          
            }
  
          } else if (speaking == true) {

              debug(F("Voice val: %4f\n"), val);
              
              if (val < VOICE_STOP) {
    
                // If the user has stopped talking for at least the required "silence" time and 
                // the mic/line input has fallen below the minimum input threshold, play a random 
                // sound from the card.  NOTE:  You can adjust the delay time to your liking...
                // but it should probably be AT LEAST 1/4 second (250 milliseconds.)
    
                if (stopped >= SILENCE_TIME) {
                  debug(F("Voice stop: %4f\n"), val);
                  voiceOff();
                  // play random sound effect
                  addSoundEffect();
                  // resume loop
                  loopOn();
                }
    
              } else {
                  
                  // Reset the "silence" counter 
                  stopped = 0;
                  
              }
    
            }
          
         }
  
  
    }
  
    readVolume();

  }
  
}

/*********************************************************
 * UTILITY FUNCTIONS
 *********************************************************/

/**
 * Convert array of char strings to comma-delimited string 
 */
char *arrayToString(char result[], const char arr[][SETTING_ENTRY_MAX], int len) 
{
  for (int i = 0 ; i < len; i++) {
    if (i == 0) {
      strcpy(result, arr[i]);
    } else {
      strcat(result, ",");
      strcat(result, arr[i]);
    }
  }
  return result;
}

/**
 * Convert array of char strings to comma-delimited char-based string 
 */
char *arrayToStringJson(char result[], const char arr[][SETTING_ENTRY_MAX], int len) 
{
  strcpy(result, "[");
  for (int i = 0 ; i < len; i++) {
    strcat(result, "\"");
    strcat(result, arr[i]);
    strcat(result, "\"");
    if (i < len-1) {
      strcat(result, ",");
    }
  }
  strcat(result, "]");
  return result;
}

/**
 * Convert array of int to comma-delimited string 
 */
char *arrayToString(char result[], int arr[], int len) 
{
  char buf[20];
  for (int i = 0 ; i < len; i++) {
    sprintf(buf, "%d", arr[i]);
    if (i == 0) {
      strcpy(result, buf);
    } else {
      strcat(result, ",");
      strcat(result, buf);
    }
    memset(buf, 0, sizeof(buf));
  }
  return result;
}

/**
 * Convert array of float to comma-delimited string
 */
char *arrayToString(char result[], float arr[], const int len) 
{
  char buf[20];
  for (int i = 0 ; i < len; i++) {
    dtostrf(arr[i], 0, 4, buf);
    if (i == 0) {
      strcpy(result, buf);
    } else {
      strcat(result, ",");
      strcat(result, buf);
    }
    memset(buf, 0, sizeof(buf));
  }
  return result;
}

/**
 * Delete the specified file
 */
boolean deleteFile(const char *filename) 
{
  if (SD.exists(filename)) {
    SD.remove(filename);
    return true;
  } else {
    return false;
  }
}

/**
 * Open a file on the SD card in the specified mode.
 */
File openFile(const char *filename, int mode) 
{
  // Thanks to TanRu !
  if (mode == FILE_WRITE) {
    deleteFile(filename);
  }
  debug(F("Opening file %s\n"), filename);
  return SD.open(filename, mode);  
}

/***
 * Show a text file
 */
void showFile(const char *filename) {
  debug(F("Showing file %s\n"), filename);
  File file = SD.open(filename);
  if (file) {
    char c;
    while (file.available()) {
      c = file.read();
      Serial.print(c);
    }
    file.close();
  } else {
    Serial.println(F("Could not find file!"));
  }
}

/**
 * Send output to BLE
 */
void btprint(const char *str) {
  if (ECHO == true) {
    Serial.print("TX: ");
    Serial.println(str);
  }
  if (BT_CONNECTED == true) {
    Serial1.print(str);
  }
}

/**
 * Send output to BLE
 */
void btprintln(const char *str) {
  if (ECHO == true) {
    Serial.print("TX: ");
    Serial.println(str);
  }
  if (BT_CONNECTED == true) {
    Serial1.print(str);
    Serial1.print("\n");
  }
}

void btprint(const __FlashStringHelper *fmt, ... ) {
  char buf[1025]; // resulting string limited to 1M chars
  va_list args;
  va_start (args, fmt);
#ifdef __AVR__
  vsnprintf_P(buf, sizeof(buf), (const char *)fmt, args); // progmem for AVR
#else
  vsnprintf(buf, sizeof(buf), (const char *)fmt, args); // for the rest of the world
#endif
  va_end(args);
  if (ECHO == true) {
    Serial.print(buf);
  }
  Serial1.print(buf);
}

/**
 * Shortcut to send output to in JSON format
 */
void sendToApp(const char *cmd, const char *value) 
{
  if (value[0] == '[') {
    btprint(F("{\"cmd\":\"%s\",\"data\":%s}\n"), cmd, value);
  } else {
    btprint(F("{\"cmd\":\"%s\",\"data\":\"%s\"}\n"), cmd, value);
  }
}

/**
 * Uses the "F" FlashStringHelper (to help save memory)
 * to output a formatted string.
 * This code is adapted from http://playground.arduino.cc/Main/Printf
 */
void debug(const __FlashStringHelper *fmt, ... ) {
  if (DEBUG != true) {
    return;
  }
  char buf[256]; // resulting string limited to 1000 chars
  va_list args;
  va_start (args, fmt);
#ifdef __AVR__
  vsnprintf_P(buf, sizeof(buf), (const char *)fmt, args); // progmem for AVR
#else
  vsnprintf(buf, sizeof(buf), (const char *)fmt, args); // for the rest of the world
#endif
  va_end(args);
  Serial.print("> ");
  Serial.print(buf);
}

/**
 * Perform a restart without having to cycle power
 */
#define RESTART_ADDR       0xE000ED0C
#define READ_RESTART()     (*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val) ((*(volatile uint32_t *)RESTART_ADDR) = (val))

void softreset() {
 WRITE_RESTART(0x5FA0004);
}

// END


