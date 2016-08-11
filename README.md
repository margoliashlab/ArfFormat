# Arf Format

A plugin for the [Open-Ephys GUI](https://open-ephys.atlassian.net/wiki/display/OEW/Home) that allows saving files in the HDF5-based [Arf](https://github.com/melizalab/arf) format.


## Installing Open-Ephys and ArfFormat

This guide is written primarily for Ubuntu 16.04, but should work on other versions of Ubuntu and other Debian-like distribuation, as well as other versions of Linux, using the different package manager. It's also good to consult Open-Ephys own [guide to installing the GUI](https://open-ephys.atlassian.net/wiki/display/OEW/Linux)

First we want to get the source code for both Open-Ephys and the plugin. Getting the GUI code from margoliashlab Github account is to guarantee that the version will work well with the plugin.

However, if you're using an older computer (for example you have only two cores), then you want to use the older version of the GUI, 0.4.0, which works better in that setting. Then also get the plugin made for that version, so do:

```
git clone https://github.com/margoliashlab/plugin-GUI.git
cd plugin-GUI/Sources/Plugins
git clone https://github.com/margoliashlab/ArfFormat.git
cd ArfFormat
git checkout 0.4.0
cd ../../..
```

Otherwise, you just need to do:

```
git clone https://github.com/margoliashlab/plugin-GUI.git
cd plugin-GUI/Sources/Plugins
git clone https://github.com/margoliashlab/ArfFormat.git
cd ../..
```

Then we install the dependencies necessary for the GUI, ZeroMQ and HDF5. 
```
sudo Resources/Scripts/install_linux_dependencies.sh
sudo add-apt-repository ppa:chris-lea/zeromq
sudo apt-get update
sudo apt-get install libzmq3-dbg libzmq3-dev libzmq3
sudo apt-get install libhdf5-dev libhdf5-cpp-10
```

Now we want to compile the GUI.

```
cd Builds/Linux
```

On some versions of Linux there is a bug that causes the GUI to freeze when you try to open a file or select a directory. To avoid that, edit the lines in files Makefile and Makefile.plugins in the current directory (there should be 2 such lines in each file) from
`CPPFLAGS := $(DEPFLAGS) -D “LINUX=1” -D “DEBUG=1” ...`
to
```
CPPFLAGS := $(DEPFLAGS) -D “LINUX=1” -D “JUCE_DISABLE_NATIVE_FILECHOOSERS=1” -D “DEBUG=1
```
and add a line 
`JUCE_DISABLE_NATIVE_FILECHOOSERS=1`
anywhere in both files.

Then you can compile.
```
make
make -f Makefile.plugins
```

## Developer notes

It's best to first look at ArfRecording.cpp to understand the general flow, and then look how particular functions are implemented. The code is a modified version of the KwikFormat plugin, so you can compare with that, as a lot of code stayed the same.

In this plugin, there are added a few more specialized functions that save to HDF5 datasets, for example a simpler function to save to one-dimensional datasetes, or to save Compound Type datasets. In particular, I didn't maintain doing everything through the ArfFileBase class, sometimes there are calls directly to the HDF5 C++ API, but that actually makes it more transparent. To understand, it's worth going through the HDF5 tutorial or examples, to understand the memory space, file space, chunking and so on.

A few things that might be surprising:

- I was not sure how to create Compound Datatypes on the fly, as the function `addEventType` would require. Therefore I implemented the two that were mentioned in the Kwik plugin. However, if different event types are ever encountered, it's easy to add them; you need to modify three places:
    1. add another struct to `ArfFile` in ArfFileFormat.h
    2. create a Compound Type in `ArfFile::addEventType` in ArfFileFormat.cpp
    3. add another `addEventType` call in `ArfRecording::openFiles` in ArfRecording.cpp
    4. add another declaration and an else-if in `ArfFile::writeEvent` in ArfFileFormat.cpp

- There is a parameter `MAX_TRANSFORM_SIZE` that limits the length of an array that represents the waveform of a spike. However, it seems that usually not the entire array is filled with data. But because variable-length datatypes inside Compound Datatypes seem problematic, I allocate and write the entire array, filling the rest with 0s. Thus the attribute valid_samples represents how many rows are actually meaningful.

- Data can be saved in parts. First, to an intermediate buffer partBuf which is an array of JUCE Arrays (so that it's easy to remove first X elements when we write them to a file) for each channel, though this might be theoretically inefficient. In case it's necessary, it shouldn't be hard to change that to a JUCE HeapBlock or something. Then, when all Arrays of all channels have the required number of samples (variable `savingNum`), we save them to the file. Also, every `cntPerPart` times we do that, we create an entirely new file with increased `partNo`. (That's in `ArfRecording::writeData`.) I also created two locks, but it's not clear to me if they are necessary. The `partBuffer` Array is locked everytime we remove from it and save to a file, so that other channels can't add their data in the middle of that. There is also a general lock `partLock`, which is locked everytime new part is being opened, but also when we try to write events or spikes. This is to prevent writing events to a file that's currently closed. You can modify how often you want to save by changing the constant `CNT_PER_PART` in `Sources/Plugins/ArfFormat/RecordControl/ArfRecording.cpp`. `SAVING_NUM` is set to 20000, and it probably shouldn't be changed, so for example if `CNT_PER_PART = 1000`, then you will save every 500 seconds on 40 kHz data.
