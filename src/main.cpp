#include "rayplayer/rayplayer.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <random>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <vector>

#include <rcamera.h>
#include <vector>

static Color shadingColor;

std::vector<uint8_t> audio_buf;
size_t audio_pos;

void audio_callback(void *buffer, unsigned int frames) {
  memcpy(buffer, audio_buf.data() + audio_pos, frames);

  audio_pos += frames;
}

void merge(int arr[], int left, int mid, int right) {
  int i, j, k;
  int n1 = mid - left + 1;
  int n2 = right - mid;

  // Create temporary arrays
  int leftArr[n1], rightArr[n2];

  // Copy data to temporary arrays
  for (i = 0; i < n1; i++)
    leftArr[i] = arr[left + i];
  for (j = 0; j < n2; j++)
    rightArr[j] = arr[mid + 1 + j];

  // Merge the temporary arrays back into arr[left..right]
  i = 0;
  j = 0;
  k = left;
  while (i < n1 && j < n2) {
    if (leftArr[i] <= rightArr[j]) {
      arr[k] = leftArr[i];
      i++;
    } else {
      arr[k] = rightArr[j];
      j++;
    }
    k++;
  }

  // Copy the remaining elements of leftArr[], if any
  while (i < n1) {
    arr[k] = leftArr[i];
    i++;
    k++;
  }

  // Copy the remaining elements of rightArr[], if any
  while (j < n2) {
    arr[k] = rightArr[j];
    j++;
    k++;
  }
}

// The subarray to be sorted is in the index range [left-right]
void mergeSort(int arr[], int left, int right) {
  if (left < right) {

    // Calculate the midpoint
    int mid = left + (right - left) / 2;

    // Sort first and second halves
    mergeSort(arr, left, mid);
    mergeSort(arr, mid + 1, right);

    // Merge the sorted halves
    merge(arr, left, mid, right);
  }
}

int main(void) {
  Player player = Player();

  std::vector<Texture> images;

  for (long i = 0; i < player.frame_num(); i++) {
    images.push_back(LoadTextureFromImage(player.fetch_next_image()));

    // auto aud = player.fetch_next_audio();
    // for (uint8_t a : aud) {
    //   audio_buf.push_back(a);
    // }
    printf("frame %ld\n", i);
  }

  std::vector<int> a;
  a.resize(player.frame_num());
  std::iota(a.begin(), a.end(), 1);

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(a.begin(), a.end(), g);

  int *arr = a.data();
  mergeSort(arr, 0, a.size() - 1);

  for (int i = 0; i < a.size(); i++)
    printf("%d ", arr[i]);
  int i = 0;

  SetConfigFlags(FLAG_MSAA_4X_HINT); // Enable Multi Sampling Anti Aliasing 4x
                                     // (if available)
  SetTraceLogLevel(LOG_NONE);
  InitWindow(1024, 768, "Project Newsroom");

  SetTargetFPS(
      60); // Set our game to run at 60 frames-per-second
           //--------------------------------------------------------------------------------------

  // Main game loop
  // while (!WindowShouldClose()) // Detect window close button or ESC key
  // {

  //   ClearBackground(BLACK);

  //   auto tv = images.at(a.at(i));

  //   DrawTexturePro(
  //       tv, (Rectangle){0, 0, (float)tv.width, (float)tv.height},
  //       (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
  //       (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

  //   EndDrawing();

  //   // UnloadTexture(tv);

  //   i += 1;

  //   //----------------------------------------------------------------------------------
  // }

  // CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
