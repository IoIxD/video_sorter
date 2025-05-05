#include "raylib.h"
#include "raymath.h"
#include "rayplayer/rayplayer.hpp"
#include <algorithm>
#include <numeric>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#define FRAME_NUM player.frame_num()

int frame_max = 0;
// #define FRAME_NUM 512

#define MAX_SAMPLES 2541
#define MAX_SAMPLES_PER_UPDATE 4096

std::vector<int> a;

std::vector<Image> textures;
int leftoff = 0;

// Cycles per second (hz)
float frequency = 100.0f;

// Audio frequency, for smoothing
float audioFrequency = 440.0f;

// Previous value, used to test if sine needs to be rewritten, and to smoothly
// modulate frequency
float oldFrequency = 1.0f;

// Index for audio rendering
float sineIdx = 0.0f;

// Audio input processing callback
void AudioInputCallback(void *buffer, unsigned int frames) {
  audioFrequency = frequency + (audioFrequency - frequency) * 0.95f;

  float incr = audioFrequency / 44100.0f;
  short *d = (short *)buffer;

  printf("%d\n", frames);

  for (unsigned int i = 0; i < frames; i++) {
    auto n = i + leftoff;
    if (n >= frame_max) {
      leftoff = 0;
      n = i;
    }
    d[i] = a[n] + (short)(16000.0f * sinf(2 * PI * sineIdx));
    sineIdx += incr;
    if (sineIdx > 1.0f)
      sineIdx -= 1.0f;
  }
  leftoff = frames;
}

// Merges two subarrays of arr[].
// First subarray is arr[left..mid]
// Second subarray is arr[mid+1..right]
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

  // draw what we have so far
  BeginDrawing();
  ClearBackground(BLACK);

  auto num = a.at(k);

  if (num < a.size()) {
    auto img = textures.at(num);
    auto tex = LoadTextureFromImage(img);

    ClearBackground((Color){
        .r = (uint8_t)num,
        .g = 0,
        .b = 0,
        .a = 255,
    });
    DrawTexturePro(
        tex, (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
        Vector2Zero(), 0.0, WHITE);
  }

  for (int i = 0; i < a.size(); i++) {
    auto y = a.at(i);
    Color col;
    if (i == k) {
      col = LIME;
    } else {
      col = WHITE;
    }
    DrawLine(GetScreenWidth() - i, y, GetScreenWidth() - i, GetScreenHeight(),
             WHITE);
  }

  EndDrawing();

  usleep(10);
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

int main() {

  auto player = Player();

  frame_max = FRAME_NUM;
  InitWindow(FRAME_NUM, FRAME_NUM * 0.75, "What");

  InitAudioDevice(); // Initialize audio device

  // Init raw audio stream (sample rate: 44100, sample size: 16bit-short,
  // channels: 1-mono)
  AudioStream stream = LoadAudioStream(44100, 16, 1);

  SetAudioStreamCallback(stream, AudioInputCallback);

  SetAudioStreamBufferSizeDefault(FRAME_NUM);

  textures.resize(FRAME_NUM);
  for (int i = 0; i < FRAME_NUM; i++) {
    textures[i] = player.fetch_next_image();
    printf("%d / %ld\r", i, FRAME_NUM);
  }

  a.resize(FRAME_NUM);
  std::iota(a.begin(), a.end(), 1);

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(a.begin(), a.end(), g);

  PlayAudioStream(
      stream); // Start processing stream buffer (no data loaded currently)

  int *arr = a.data();
  int n = FRAME_NUM - 1;
  // Sorting arr using mergesort
  mergeSort(arr, 0, n - 1);

  // for (int i = 0; i < n; i++) {
  //   printf("%d ", arr[i]);
  // }

  while (!WindowShouldClose()) {
  }

  return 0;
}