# UEnes
 Nestopia wrapper plugin for unreal

## Install
- Make a "UEnes" folder in your game's plugins folder
- Copy contents to Plugins/UEnes

## Usage
- Add an instance of `BP_NesDemo` to your level
- Point the `BP_NesDemo` instance's `Rom Path` property to an nes ROM on your computer

### Known Issues
- Emulation framerate cane be choppy when unreal is running at a framerate that isn't a multiple of 60
- Sound crackles from time to time
