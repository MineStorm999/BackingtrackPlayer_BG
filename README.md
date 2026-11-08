# Backing Track Player
### Features
- Multi (N) Channel support **TODO**
    - Also Multiple Channels in and then routed to one channel (or left out)
- Internally uses f32 for easear programming **TODO** switch to 24bit processing
### Libraries used
- [Mini Audio](https://github.com/mackron/miniaudio) for wav/mp3/flac file streaming and multichannel playblack
- [JSON](https://github.com/nlohmann/json) for human readable config

## Docs
### JSON Config 
```
"Files":[
    {
        "path":"path/to/audio/file", 
        "volume":1.0,
        "routing":[
            [0,1,2,3,4,5,6,7], // route channel 0 to 0-7 out
            [0,1,2,3], // route channel 1 to 0-3 out
        ]
    },
]
```