This repo is to generate a C Header file that will be usable with the GameTank C APK.
The data is taken from a .JSON file that is created from the [Tiled Level Editor](https://github.com/mapeditor/tiled).

## Build Project
Make sure to install the submodules, as this depends on [cJSON](https://github.com/davegamble/cjson) being in the 'third_party/cJSON' folder.
```
$ git submodule update --init
$ make
```

## Usage
* First, download the Tiled map editor.
* Create your map as you like.
* When ready, export your map using "Export As...", and name it with a .json extension. It **must** be .json.
* With the .json, and the built project executable from running "make", drag the .json file on-top of the executable.
* The executable will then create a .h file beside the .json file. It will also create a log.txt, which will contain any errors that might occur.

## Todo
This project is unfinished as it is. It is currently the stepping stone for me to make levels in the GameTank SDK. 
This application will evolve as I use it further.
