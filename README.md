# Backing Track Player
## Features
- Multi (N) Channel support **TODO**
    - Also Multiple Channels in and then routed to one channel (or left out)

## Libraries used
- [Mini Audio](https://github.com/mackron/miniaudio) for wav/mp3/flac file streaming and multichannel playblack
- [JSON](https://github.com/nlohmann/json) for human readable config

## Docs
### JSON Config
```
{
    "outputs":{
        "1":{
            {
                "path":"path/to/wav",
                "volume":1.0
            },
            {
                "path":"path/to/wav2",
                "volume":0.4
            }
        },
        "3":{
            {
                "path":"path/to/wav",
                "volume":1.0
            },
            {
                "path":"path/to/wav2",
                "volume":0.4
            }
        }
        ...
    }
}
```