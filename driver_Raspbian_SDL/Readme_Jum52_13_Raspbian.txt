Jum's A5200 Emulator, Raspbian/SDL Version 1.3
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

27 September 2018

This is an Atari 5200 emulator for Raspberry Pi.

The 6502 CPU emulator source is heavily based on a distribution
by Neil Bradley. The POKEY sound emulator is a modified version 
of Ron Fries POKEY emulator.
The rest is by me.


Obligatory Copyright Notice:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Jum's A5200 Emulator is copyright 1999-2018 by James Higgs.
POKEY Sound is copyright 1996 by Ron Fries.

Jum's A5200 Emulator is free as long as it is not used in a commercial
matter and not altered in any way. The contents of this archive should
not be added to or changed in any way. 

I maintain the right to forbid the use of the emulator at
any time. I am not responsible for any damage caused by the use
of this program. This program is distributed "as-is". I make no
guarantees as to it's accuracy, performance, or compatibility with
the user's hardware.

Jum's A5200 Emulator ("Jum52") is not to be included in commercial
emulator packages of any sort.


******************************************************************
	DO NOT ASK ME FOR ROM OR CARTRIDGE IMAGES !!!
******************************************************************

Changes (v1.1 to v1.3):
~~~~~~~

- Fix to synchronising of audio buffer (less crackly audio)
- Fix to keyboard input
- Robotron mode for PC dual-stick gamepad (or PS3/PS4 controller)



Compatibility:
~~~~~~~~~~~~~~

Most carts will run.

These games give problems:
- Decathlon
- Mr. Do's Castle (corrupt rom dump)
- Quest for Quintana Roo
- Rescue on Fractalus
- Ballblazer (very difficult to start a game)




Keyboard Controls:
~~~~~~~~~~~~~~~~~~

Esc		Go to Options Menu
F1		5200 Start button
F2		5200 Pause button
F3		5200 Reset button
F4		Go to monitor/debugger
F5		5200 * button
F6		5200 # button
F11		Show fps
F12		Take a snapshot "snap.bmp"



Player 1:
~~~~~~~~~
Arrow Keys	Up/Down/Left/Right
Right Ctrl	Fire
Right Shift     Side trigger ("fire 2")

Player 2:
~~~~~~~~

E/D/S/F		Up/Down/Left/Right
Left Ctrl	Fire
Left Shift      Side trigger ("fire 2")
Delete		* button (P2)
Return		# button (P2)


Note: Keys can be changed using key remapping (see below).



Joystick Modes:
~~~~~~~~~~~~~~

From the Options Menu, select joystick mode by toggling "Controller" (press left or right arrow key while on the Controller option).

Pengo control mode is automatically activated when Pengo cart is loaded.

Many games do "auto-calibrating" while you play. Moving the joystick handle in a big circle while chanting "work dammit" usually gets it working OK. Also it sometimes helps if you leave the analog stick in the central position when starting a game.

If you activate joystick mode, and the game does not control as
expected, then try reloading the game.

Originally only standard 2-axis / 2-button joystick and a MS Sidewinder were supported. Recent support for "generic" dual analog PC controllers has been added. The D-PAD on the controller should also work in joystick mode.




Mouse Mode:
~~~~~~~~~~

This is useful for playing Missile Command or other trackball games.

From the Options Menu, select "Mouse" mode by toggling "Controller" (press left or right arrow key while on the Controller option).



MousePaddle Mode:
~~~~~~~~~~~~~~~~

This is useful for games that use a paddle, such as:
KABOOM!
Gorf
Breakout

From the Options Menu, select "MousePaddle" mode by toggling "Controller" (press left or right arrow key while on the Controller option).



Getting started:
~~~~~~~~~~~~~~~

You will need:
- A PC
- Some 16k or 32k Atari 5200 cartridge images

1. Copy the cartridge images into the same directory as the
   Jum52 executable file.
2. Run Jum52
3. Have fun!



Configuring Jum52:
~~~~~~~~~~~~~~~~~

You can set options for Jum52 by editing the jum52.cfg file. The 
options you can change are:
    - audio output (on/off)
    - voice emulation (on/off)
    - audio volume
    - fullscreen mode (on/off)
    - default controller (keyboard/joystick/mouse)
    - default control mode (normal/robotron/pengo)
    - video output scale (1/2)
    - alternate palette file
    - slow-motion mode (on/off)
    - extra (custom) cartridge mappings
    - keyboard remappings

1. Make a copy of jum52.cfg (call it "jum52.cfg.bak" or something).
2. Edit jum52.cfg with a text editor.
3. Set the options as you prefer (valid options are in brackets).
4. Save jum52.cfg.

You can also add mappings for "unknown" 16k roms (eg: if you're
writing a homebrew game and you want to test it in Jum52 :). In this
case, take a look at the sample mappings in jum52.cfg. You can get 
the CRC of the rom by running it in Jum52, then looking in the 5200.log file. 


Keyboard Remapping:
~~~~~~~~~~~~~~~~~~

You can remap the keys used in Jum52 by adding key mappings to the
jum52.cfg file. The format is:

keymap=<in code>,<out code>

# where <in code> and <out code> are ASCII or extended key codes 
(code 0 to code 320).

eg: To use the "Home" key as the "Start" key for Jum52, we need to 
remap "Home" key (code 278) to the "F1" key (code 282):

keymap=278,282

(You can get a list of the key codes by Googling "sdl_keysym.h").



The debugger:
~~~~~~~~~~~~

*** Skip this is you're not a 5200 programmer :) ***

The debugger has functions that allow you to stick your fingers into 
the moving parts of the 5200 while it is running:
- Info on the 6502, ANTIC, GTIA registers
- 6502 disassembly with nice 5200 hw reg info
- step thru 1 instruction, 1 scanline, or 1 frame
- run to an address
- view the current character set and PM graphics data
- view the collision registers
- view the DLIST data (sorry no DLIST disassembler :{ )
- view the interrupt vectors
- view data at an address
- some other stuff useful to programmers

To enable the debugger, set the "debug" option in jum52.cfg to "yes", 
then restart Jum52.

To get to the debugger while running a game, press F4.

Press 'h' when in the debugger to get a list of available commands.



******************************************************************
	DO NOT ASK ME FOR ROM OR CARTRIDGE IMAGES !!!
******************************************************************



FAQ:
~~~~

1. Q: How do I make the Jum52 window bigger?
   A: Edit jum52.cfg as change "scale=1" to "scale=2", OR
   A: Edit jum52.cfg as change "fullscreen=no" to "fullscreen=yes".

2. Q: It doesn't run on my Mac
   A: Get the Mac version from www.bannister.org
      (It's also available for Windows, PS2, QNX and BeOS)

2. Q: It's crap. There's no blah blah yadda yadda ...
   A: It's free. Waddaya expect?

3. Q: It's so sssslllllooooowwwwwwwww...........
   A: Your PC is crap.
   A: Set 'scale=1' in jum52.cfg
   A: Run your Windows emulator on a faster PC.

4. Q: I have trouble getting my joystick to work.
   A: It happens to even the best of men.
   A: Make sure that it's set up properly in Windows Control Panel.

5. Q: I don't hear any sound.
   A: Cut down on the heavy metal.
   A: Turn up the volume REALLY loud.
   A: Check the volume setting in the Jum52's Options Menu
   A: Check that 'audio=on' is in jum52.cfg

6. Q: Game X doesn't work.
   A: It may be a corrupt/bad ROM image (there are many).
   A: If you can't get past the start screen in the game,
      then Jum52 just doesn't handle that game (yet).
   A: If it's a 16k rom, try adding a mapping for it in jum52.cfg

8. Q: When I select "Load Game", nothing is listed.
   A: Copy your Atari 5200 game roms (with a .BIN file extension) to 
      the same folder as Jum52.

9. Q: Where can I get ROMZ? (plead/whine/grovel/demand)
   A: Learn to use a search engine, or something.



Troubleshooting and Comments:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Email:  james7780@yahoo.com

1. Examine the 5200.log file for errors.
2. RTFM ("Getting started" above)
3. Intelligent questions are welcome.
4. Constructive comments are appreciated (especially comments on
   how the emulator differs from the real thing).


Future Features:
~~~~~~~~~~~~~~~~

1. Better.
2. Faster.
3. Better controller support.
4. Whatever you can suggest? 


Credits:
~~~~~~~~
Thanks to:
Dan Boris (author of VSS and V7800) for infos.
Ron Fries (for POKEY emu).
Neil Bradley for 6502 emu.
Sherwood for helpful comments and other stuff.
Christopher Durante for useful input.
Richard Bannister for cross-platform conversion and Mac version.
John Swiderski.
Calamari (5200Bas).
5200 homebrew devers.
Other people who helped, contributed, cajoled or complained.

