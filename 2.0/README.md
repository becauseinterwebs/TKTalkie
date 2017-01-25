# TKTalkie v2.0

The TKTalkie software is part of the TKTalkie DIY project.  This revision has some significant changes designed to making it easier to configure and use the system WITHOUT having to recompile and upload the software to the Teensy board and contains the following improvements:

  - Text config file allows you to customize settings and options without having to recompile the code
  - Calibration wizard added to help find optimum settings for your particular use
  - Added support for an endless loop (background chatter) file
  - Added the ability to use a serial connection (via Arduino IDE Serial Monitor or Terminal software) to change settings, save config files and load config files to enable easier testing and the use of different configs for different applications
  - Cleaned up and optimized the code

## Configuration File

You can now have TKTalkie load configuration settings from a file on the SD card.  This is the preferred method and allows you the flexibility of using the system in more than one application.

The config file is named TKCONFIG.TXT and has the following format for configuration values:

    [setting_key=setting_value]  *Note the use of [ and ] for the settings!
    
> Settings are wrapped in brackets due to the differences in the way editors will use the EOL (End-of-Line) character.  This method allows the use of any editor for your configuration file.

> **NOTE**: Any text *OUTSIDE* of the brackets will be ignored.

The current settings and valid values are:

**linein**

Set the level for the line-in input. Valid values are 0 to 15, with 5 being the default. Lower numbers decrease the input level, higher numbers increase it. 

**lineout**

Set the level for the line-out signal.  Valid values are 13 to 31, with 29 being the default.  Lower numbers increase the level, higher numbers decrease it.

**startup**

This is the sound that is played each time TKTalkie starts.

    # sound to play when TKTalkie is started
    [startup=STARTUP.WAV]

**loop**

The background (chatter) file.  This file starts AFTER the startup file, then will continuously loop while TKTalkie is running.

    # chatter loop settings
    [loop=CHATTER.WAV]

> **NOTE**: The backgound loop will be silenced while you are speaking (but will continue running)

**loop_gain**

This is the sound level of the loop while it is playing.

    # 0 to 32767, 1 is pass-thru, below 1 attenuates signal
    [loop_gain=7]
    
**silence_time**

The amount of time (in milliseconds...or *thousandths* of a second) to wait to make sure you are finished talking before playing an effect (static burst or mic click effect, etc.)

    # VOICE ACTIVATION SETTINGS
    [silence_time=350]

**voice_start**   (*formerly VOL_THRESHOLD_TRIGGER*)

This is the input level received from the microphone that will trigger the Voice Activation when you begin talking.

    # valid values are any decimal value between 0.00 and 1.00
    [voice_start=0.07]
    
> **NOTE"": You can use the new Calibration Wizard via the serial interface to help set this level

**voice_stop**  (*formerly VOL_THRESHOLD_MIN*)

The input level received from the microphone that indicates you are finished talking.

    [voice_stop=0.02]

> **NOTE**: You can use the new Calibration Wizard via the serial interface to help set this level

*The following entries are for using the PTT (Push-to-Talk) function*

**button_pin**

This setting tells TKTalkie which pin on the Teensy your PTT button is connected to.

    # PTT (Push-To-Talk) SETTINGS
    [button_pin=2]
    
**button_click**

The sound to play when the PTT button is pushed (before you start talking.)  When the button is released, a random sound will be played.

    [button_click=BUTTON.WAV]

**button_gain**

The output level of the button sound.  

    # 0 to 32767, 1 is pass-thru, below 1 attenuates signal
    [button_gain=1]
   
**input**

This setting specifies which voice input will be used, the microphone input or the line-in input. Default is microphone.

    # MICROPHONE/LINE-IN SETTINGS
    # input settings (1 = microphone, 0 = line-in)
    [input=1]

**mic_gain**

The voice input level when using the microphone input.

    # 0 to 63
    [mic_gain=5] 

> **NOTE**: If you are experiencing feedback, try adjusting the *mic_gain* setting first.

**effects_gain**

This sets the output level of sound effects.

    # SOUND EFFECTS (STATIC BURSTS, ETC.)
    # 0 to 32767, 1 is pass-thru, below 1 attenuates signal
    [effects_gain=5]

**eq**

Specify the equalizer type to use.

    # EQUALIZER SETTINGS
    # 0 = flat (none, 1 = parametric, 2 = bass/treble, 3 = graphic
    [eq=3]
    
**eq_bands**

Depending upon they type of equalizer selected, specify the settings for each band.  Bands are comma separated and valid decimal values from -1.00 to 1.00.

    # for parametric/graphic = 5 bands, for bass/treble = 3 bands
    # bands are low to high: -1 (-11.75dB) to 1 (+12dB)
    [eq_bands=-1,0,1,0,-1]

**voice_gain**

Set the output level of the voice channel on the mixer.

    # VOICE SETTINGS
    # 0 to 32767, 1 is pass-thru, below 1 attenuates signal
    [voice_gain=1]

**bitcrushers**

There are two bitcrushers to process the voice input and make it sound a little more tinny and robotic as if it is coming through a radio.  These are comma separated values.  Valid values for bits are 1 to 16, and valid values for rate are from 1 to 44100.

    # BITCRUSHER SETTINGS - VOCAL EFFECTS
    # Format = bits1,rate1,bits2,rate2
    # Set to 16,41000,16,41000 to just pass-thru (disable)
    [bitcrushers=12,16384,10,10240]

> **NOTE**: You may need to disable these if using other voice processers such as a voice changer with TKTalkie.

**noise_gain**

While talking, pink noise is played behind your voice to help simulate radio communications.  This setting sets the noise level.  Set to 0 to disable.

    # PINK NOISE GENERATOR
    # 0 to 32767, 1 is pass-thru, below 1 attenuates signal
    [noise_gain=1]

**debug**

Turns the debug setting on or off.  Default is off.  When on, messages during program operation will be displayed to the serial interface.  This can be useful when testing configuration settings.

    # Turn debug on or off.  Valid values are 1 (on) or 0 (off.) 
    debug=0
    
## Serial Interface

This version of the sofware allows you to change configuration settings, save configuration files and load configuration files via a serial interface.  The Arduino IDE has a built-in serial monitor that can be used for this, or you can connect via a terminal program at 9600 baud.

When connected, the following commands are available:

    backup           Quick backup of in-memory settings to SETTINGS.BAK
    restore          Quick restore of SETTINGS.BAK file
    save             Saves the current in-memory configuration to the default TKCONFIG.TXT file
    load             Loads the configuration values fromt the default TKCONFIG.TXT file
    save=filename    Saves the current in-memory configuration to the specified file
    load=filename    Loads the configuration values in the specified file
    files            Displays a list of all files on the SD card
    calibrate        Starts the calibration wizard
    settings         Displays the current settings
    help             Displays the help screen
    play=filename    Plays the specified .WAV file (use full filename)
    
## Changing Configuration Settings

There are now three ways to change configuration settings:

- Change the default values in the source code and recompile and upload the code to your device
- Change configuration values in the TKCONFIG.TXT file and restart the device
- Make live changes via the serial interface and then save them

### Making Configuation Changes via Code

If you do not wish to use a config file, you can simply alter the default options located at the top of the Arduiono sketch and recompile, then upload, your changes to your device.

### Making Configuration Changes via Config File

With this revision, all configuration options are now loaded from a config file, called TKCONFIG.TXT, located on the SD card.  Using this option, you can simply edit this file, save your changes, then reinsert the card into the Teensy device and restart. Please see the [Configuration File](#Configuration File) section for a list of available options.

### Making Configuration Changes via Serial Interface

You can now make live, real-time changes via the serial interface.  Changes take effect immediately.  They will be LOST when the device restarts unless you save them using one of the '*save*' commands.  When making live changes, use the following format:

    setting_key=new_value [ENTER]
    
The new setting will take effect immediately.  This is a great way to allow you to tweak and test settings for your particular application of the TKTalkie system.  Please see the [Configuration File](#Configuration File) section for a list of available options.
    
For more information regarding TKTalkie, please visit [www.tktalkie.com](http://www.tktalkie.com).
