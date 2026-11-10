#include "Track.h"

class Song {
public:
  Song(nlohmann::json &config) { FromFile(config); }

  void FromFile(nlohmann::json &config) {
    if (config.is_null()) { // config missing
      printf("ERROR config missing\n");
      return;
    }

    nlohmann::json &tracksJS = config["tracks"];
    if (!tracksJS.is_array()) { // files missing
      printf("ERROR files missing\n");
      return;
    }

    tracks.clear();
    tracks.reserve(tracksJS.size()); // Testing
    // int i = 0;
    for (nlohmann::json &f : tracksJS) {
      for (int i = 0; i < 1; i++) {
        tracks.push_back(std::make_shared<Track>(f));
        if (!tracks.back()->Ready()) {
          printf("Warning file setup failed/not routed, removing\n");
          tracks.pop_back();
        }
      }
    }
    tracks.shrink_to_fit();
  }

  std::vector<std::shared_ptr<Track>> tracks;
  void Play() {
    for (auto t : tracks) {
      t->Play();
    }
  }
};