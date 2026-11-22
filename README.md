# Backing Track Player
### Features
- N audio files N channel **Input** to **8 channel** *(can also be adjusted in the code)* **output**
- Start/stop, revert
- Song in one show management **TODO**
- multiple show management **TODO**
- auto load shows from usb when inserted **TODO**
- Internally uses f32 for easier programming

### Install
```
git clone https://github.com/MineStorm999/BackingtrackPlayer_BG.git
git checkout RaspiTest
cd "to/cloned/dir"
mkdir build
cd build 
cmake ..
```

the created executable should be in build/bin 
### Roadmap
[TodoList](TODO.md)

### Libraries used
- [Mini Audio](https://github.com/mackron/miniaudio) for wav/mp3/flac file streaming and multichannel playblack
- [JSON](https://github.com/nlohmann/json) for human readable config
- [Eigen](https://libeigen.gitlab.io/) for vectorisation

## Docs
### JSON Config 
***Important*** All paths **must** be *relative*  

**Song Config**
- Shows must contain .song (ie.: my_show.song.json)
```
"name":"song name",
"tracks":[
    {   // per track (file)
        "path":"path/to/audio/file",
        "volume":1.0,
        "routing":[
            [0,1,2,3,4,5,6,7], // route channel 0 to 0-7 out
            [0,1,2,3], // route channel 1 to 0-3 out
        ]
    },
    {   // per track (file)
        "path":"path/to/audio/file2", 
        "volume":1.0,
        "routing":[
            [0,1,2,3,4,5,6,7], // route channel 0 to 0-7 out
            [0,1,2,3], // route channel 1 to 0-3 out
        ]
    }
]
```
**Show Config**
- Shows Must be in top (root) level.
- Shows must contain .show (ie.: my_show.show.json)
```
"name":"show name",
"songs":[
    {
        "path":"path/to/show.config"
    },
    {
        "path":"path/to/show1.config"
    }
]
```
**Folder Structure Example**
```
root/
|-my_show.show.json
|-my_song/
  |---my_song.song.json
  |---track.wav
  |---track1.wav
  |---track2.wav
|-my_song2/
  |---my_song.song.json
  |---Tracks/
      |---track.wav
      |---track1.wav
      |---track2.wav
```

### Show Manager
```plantuml
@startuml
skinparam activity {
    ' this is a comment
    BackgroundColor White
    BorderColor Black /' this is a comment '/
    BorderColor Red  ' this is not a comment and this line is ignored
}

    class Song{
        //Song_Data
        + Valid() //return false if directory missing or no tracks
    }
    note right of Song: Implemented
    

    class Show{
        - Song m_songs[] // READ ONLY
        --
        + GetSong() // returns current Song
        + Next()
        + Prev()
        + Valid() // returns false if directory missing, or no songs
    }
    note right of Show: Implemented

    class ShowManager{
        - async_list<Library> m_libs;
        - m_libID;
        - m_showID;
        --
        + bool Next()
        + bool Prev() 
        + Show* GetShow() // returns current show
    }
    note right of ShowManager: Need To Implement

    class Library{
        - USB_DeviceRef ref
        - Show m_shows[]
        --
        + _OnRemoved()
        + _Count()
        + Show* GetShow(int id) // returns show at id  
        + Valid() // returns false, if usb dev was removed (or empty)
    }
    note right of Library: Need To Implement

    Song <-- Show
    Library <-- ShowManager
    Show <-- Library
@enduml
```

### What happens when the usb gets detached
```plantuml
@startuml
start 
:USB Detached;
:Library gets invalidated;
:Audio thread requests previous (valid) song;
:Remove the Lib;
end

@enduml
```

### What happens when the usb gets attached
```plantuml
@startuml
:USB Attached;
 fork
   :Library;
   :Loads Shows in vector (local);
   :Lock Global Show Vector and insert local;
 fork again
   :Audio thread;
   if (Play) then
   :start Manager->NextSong;
   endif
   detach;
 fork again
    :Ctrl Thread;
    if (Prev/Next) then
    :lock Manager->Shows;
    :prev/next logic (select song);
    :atomically set Manager->NextSong to selected song;
    :Maybe request restart;
    endif
 endfork
@enduml
```
