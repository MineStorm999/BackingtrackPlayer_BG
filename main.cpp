#include "Eigen/Core"
#include "Track.h"
#include "json.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <unistd.h>
#define MINIAUDIO_IMPLEMENTATION
#include "Show.h"
#include "defines.h"
#include "miniaudio.h"
#include <Eigen/Dense>
#include <iostream>
#include <udisks/udisks.h>

void ExitOnFail(ma_result result) {
  if (result != MA_SUCCESS) {
    printf("Error: %u", (uint)result);
    exit(-1);
  }
}

// Source - https://stackoverflow.com/a/28827188
// Posted by Bernardo Ramos, modified by community. See post 'Timeline' for
// change history Retrieved 2026-11-07, License - CC BY-SA 4.0

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h> // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sleep_ms(int milliseconds) { // cross-platform sleep function
#ifdef WIN32
  Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  nanosleep(&ts, NULL);
#else
  if (milliseconds >= 1000)
    sleep(milliseconds / 1000);
  usleep((milliseconds % 1000) * 1000);
#endif
}

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {
  if (!pDevice->pUserData) {
    return;
  }

  Show *show = (Show *)pDevice->pUserData;
  if (!show) {
    return;
  }
  std::shared_ptr<Song> song = show->GetSong();
  if (!song) {
    return;
  }
  if (song->MustReverse()) {
    song->ReverseTracks();
  }
  if (!song->Playing()) {
    return;
  }

  Eigen::Map<Eigen::MatrixXf> dstMat((float *)pOutput, OUT_CHANNEL_COUNT,
                                     frameCount);
  for (auto track : song->tracks) {
    if (!track) {
      continue;
    }

    track->ReadPCMFramesAndMixToOutput(dstMat, frameCount);
  }
  Eigen::VectorXf volumeVec(frameCount);

  for (int i = 0; i < frameCount; i++) {
    // volumeVec[i] = song->GetVolumeForNextSample();
    dstMat.col(i) *= std::pow(song->GetVolumeForNextSample(), 2);
  }
  // dstMat.rowwise() += volumeVec.transpose();
}

bool is_removable_device(UDisksClient *client, UDisksBlock *block) {
  if (!client || !block) {
    return false;
  }

  // 1. Physisches Laufwerk (UDisksDrive) zum Blockgerät abrufen
  // Hinweis: _get_drive_for_block erhöht den Referenzzähler!
  UDisksDrive *drive = udisks_client_get_drive_for_block(client, block);
  if (!drive) {
    // Virtuelle Geräte (z. B. Loop-Devices oder RAM-Disks) haben kein
    // UDisksDrive
    return false;
  }

  // 2. Prüfen, ob das Laufwerk wechselbar ist
  gboolean is_removable = udisks_drive_get_removable(drive);

  // Optional: Zusätzlich prüfen, ob es über USB angeschlossen ist
  // const gchar *connection_bus = udisks_drive_get_connection_bus(drive);
  // bool is_usb = (g_strcmp0(connection_bus, "usb") == 0);

  // 3. Referenz auf UDisksDrive wieder freigeben
  g_object_unref(drive);

  return is_removable == TRUE;
}

void TestUDEV() {
  GError *error = nullptr;

  // 1. Synchronen Client initialisieren
  UDisksClient *client = udisks_client_new_sync(nullptr, &error);
  if (!client) {
    std::cerr << "Fehler beim Verbinden mit UDisks2: " << error->message
              << "\n";
    g_error_free(error);
    return;
  }

  // 2. Alle verwalteten D-Bus-Objekte abrufen
  GDBusObjectManager *manager = udisks_client_get_object_manager(client);
  GList *objects = g_dbus_object_manager_get_objects(manager);

  for (GList *l = objects; l != nullptr; l = l->next) {
    UDisksObject *object = UDISKS_OBJECT(l->data);

    // Prüfen, ob das Objekt ein Blockgerät ist
    UDisksBlock *block = udisks_object_peek_block(object);
    if (block) {
      if (!is_removable_device(client, block)) {
        continue;
      }
      const gchar *device = udisks_block_get_device(block);
      guint64 size = udisks_block_get_size(block);
      std::cout << "Gerät: " << device << " (" << (size / (1024 * 1024))
                << " MB)";

      // Prüfen, ob das Blockgerät ein Dateisystem besitzt
      UDisksFilesystem *fs = udisks_object_peek_filesystem(object);

      if (fs) {
        const gchar *const *mount_points =
            udisks_filesystem_get_mount_points(fs);

        if (mount_points && mount_points[0]) {
          std::cout << " -> Eingehängt unter: " << mount_points[0];
        } else {
          std::cout << " -> Nicht eingehängt";
        }
      }
      std::cout << "\n";
    }
  }

  // 3. Speicher freigeben
  g_list_free_full(objects, g_object_unref);
  g_object_unref(client);
}

static void on_object_added(GDBusObjectManager *manager, GDBusObject *object,
                            gpointer user_data) {
  UDisksClient *client = UDISKS_CLIENT(user_data);
  UDisksObject *udisks_obj = UDISKS_OBJECT(object);

  UDisksBlock *block = udisks_object_peek_block(udisks_obj);
  UDisksFilesystem *fs = udisks_object_peek_filesystem(udisks_obj);

  // Nur reagieren, wenn es ein Blockgerät mit Dateisystem und ein Wechselmedium
  // ist
  if (block && fs && is_removable_device(client, block)) {
    const gchar *device = udisks_block_get_device(block);
    const gchar *label = udisks_block_get_id_label(block);

    std::cout << "[ANGESCHLOSSEN] Gerät: " << device
              << " | Label: " << (label && *label ? label : "Unbenannt")
              << "\n";
  }
}

// Callback: Wird aufgerufen, wenn ein Gerät entfernt wird
static void on_object_removed(GDBusObjectManager *manager, GDBusObject *object,
                              gpointer user_data) {
  UDisksObject *udisks_obj = UDISKS_OBJECT(object);
  UDisksBlock *block = udisks_object_peek_block(udisks_obj);

  if (block) {
    std::cout << "[ENTFERNT] Gerät: " << udisks_block_get_device(block) << "\n";
  }
}

void TestDisksLoop() {
  GError *error = nullptr;
  UDisksClient *client = udisks_client_new_sync(nullptr, &error);
  if (!client) {
    std::cerr << "UDisks2 Fehler: " << error->message << "\n";
    g_error_free(error);
    return;
  }

  GDBusObjectManager *manager = udisks_client_get_object_manager(client);

  // Signale mit unseren Callback-Funktionen verknüpfen
  g_signal_connect(manager, "object-added", G_CALLBACK(on_object_added),
                   client);
  g_signal_connect(manager, "object-removed", G_CALLBACK(on_object_removed),
                   client);

  // GMainLoop erstellen und in einem Hintergrund-Thread starten
  GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
  std::thread dbus_thread([loop]() { g_main_loop_run(loop); });

  std::cout << "Überwache USB-Geräte... Drücken Sie Enter zum Beenden.\n";
  std::cin.get();

  // Sauberes Beenden der Loop und Aufräumen
  g_main_loop_quit(loop);
  dbus_thread.join();

  g_main_loop_unref(loop);
  g_object_unref(client);
}

int main() {
  // TestUDEV();
  // TestDisksLoop();
  ma_result result;
  ma_decoder decoder;
  ma_device_config deviceConfig;
  ma_device device;
  std::ifstream f("config.json");
  nlohmann::json data = nlohmann::json::parse(f);

  Show show("/home/spacedviking/Dokumente/BrassGazz/BackingtrackPlayer_BG/"
            "test.show.json",
            "test.show.json");

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = ma_format_f32;
  deviceConfig.playback.channels = OUT_CHANNEL_COUNT;
  deviceConfig.sampleRate = SAMPLE_RATE;
  deviceConfig.dataCallback = data_callback;
  deviceConfig.pUserData = &show;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    ma_decoder_uninit(&decoder);
    return -3;
  }

  if (ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(&device);
    return -4;
  }
  int state = 0;
  while (true) {

    printf("Press Enter to quit...");
    getchar();
    if (state == 0) {
      song.Pause(true);
      state++;
    } else if (state == 1) {
      song.Play();
      state = 0;
    }
  }
  ma_device_uninit(&device);

  return 0;
}