#include <SDL.h>
#include <iostream>
#include <cmath>
#include <limits>

namespace
{

  struct AudioData
  {
    SDL_AudioDeviceID dev;
    size_t freq; // frequency of the audio device (e.g. 48000)

    size_t t; // time in samples from the last rest

    size_t f; // note to play: period in samples
    size_t nf; // new frequency to play as soon as possible

    size_t v; // volume
    size_t nv; // new volume asap
  };

  // this average rounds towards the target
  // so eventually it sets to the target
  size_t average(const size_t x1, const size_t x2, const size_t w1)
  {
    if (x1 == x2)
    {
      return x1;
    }

    const double y = double(w1 * x1 + x2) / double(w1 + 1);
    size_t result;
    if (x2 > x1)
    {
      // we are going up towards x2
      result = std::ceil(y);
    }
    else
    {
      // we are going down towards x2
      result = std::floor(y);
    }

    return result;
  }

  void audioCallback(void* userdata, Uint8* stream, int len)
  {
    AudioData * audioData = static_cast<AudioData *>(userdata);

    Sint16* buf = (Sint16*)stream;
    const int bytesPerSample = sizeof(Sint16) / sizeof(Uint8);

    const size_t numberOfSamples = len / bytesPerSample;
    for (size_t i = 0; i < numberOfSamples; ++i)
    {
      const size_t t = audioData->t;

      const size_t f = audioData->f;
      const size_t nf = audioData->nf;
      const size_t v = audioData->v;
      const size_t nv = audioData->nv;

      // if there is a pending change
      // and we are at the end of a period
      if ((nf != f || nv != v) && (f == 0 || t % f == 0))
      {
	// average a bit to reduce the chance of crackling
	const size_t leftWeight = 4;

	audioData->f = average(audioData->f, audioData->nf, leftWeight);
	audioData->v = average(audioData->v, audioData->nv, leftWeight);
	// and reset the counter, so "%" operates properly
	audioData->t = 0;
      }

      if (f != 0)
      {
	const double x = 2.0 * M_PI * double(audioData->t) / double(audioData->f);
	const Sint16 value = audioData->v * sin(x);
	buf[i] = value;

	++audioData->t;
      }
    }
  }

  void openAudio(AudioData & audioData)
  {
    SDL_AudioSpec want, have;

    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.channels = 1;
    want.samples = 4096;
    want.callback = &audioCallback;
    want.userdata = &audioData;

    audioData.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if (audioData.dev == 0)
    {
      std::cout << "Failed to open audio: " << SDL_GetError() << std::endl;
    }
    else
    {
      audioData.freq = have.freq;

      audioData.t = 0;
      audioData.v = 0;
      audioData.nv = 0;
      audioData.f = 0;
      audioData.nf = 0;

      SDL_PauseAudioDevice(audioData.dev, 0);
    }

  }

  // x in [0, 1]
  // output in [440 / 2, 440 * 2]
  // 4 octaves
  double interpolateFrequency(double x)
  {
    const double halfRange = 2.0;
    const double k = std::log(halfRange);
    const double mult = std::exp(2.0 * k * x) / halfRange;

    const double freq = 440.0 * mult;
    return freq;
  }

}

int main(int argc, char * argv[])
{
  const int ok = SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_AUDIO);

  if (ok)
  {
    std::cout << "Error: " << ok << std::endl;
  }

  const int joyID = 0;
  SDL_Joystick* joy = SDL_JoystickOpen(joyID);

  AudioData audioData;

  openAudio(audioData);

  if (joy)
  {
    const int numberOfAxes = SDL_JoystickNumAxes(joy);
    std::cout << "Opened Joystick " << joyID << std::endl;
    std::cout << "Name: "<< SDL_JoystickName(joy) << std::endl;
    std::cout << "Number of Axes: " << numberOfAxes  << std::endl;
    std::cout << "Number of Buttons: " << SDL_JoystickNumButtons(joy) << std::endl;
    std::cout << "Number of Balls: " << SDL_JoystickNumBalls(joy) << std::endl;

    const SDL_JoystickID id = SDL_JoystickInstanceID(joy);

    bool cont = true;

    while (cont)
    {
      SDL_Event event;

      if (SDL_WaitEvent(&event))
      {
	switch (event.type)
	{
	case SDL_JOYAXISMOTION:
	  {
	    const SDL_JoyAxisEvent & je = event.jaxis;
	    if (id == je.which)
	    {
	      switch (je.axis)
	      {
	      case 1: // volume
		{
		  const int value = je.value; // this is a Sint16

		  // this number is between 0 and 32767.5 = 32767
		  const int volume = (value - std::numeric_limits<Sint16>::min()) / 2;

		  SDL_LockAudioDevice(audioData.dev);
		  audioData.nv = volume;
		  SDL_UnlockAudioDevice(audioData.dev);
		  break;
		}
	      case 4: // freq
		{
		  const int value = je.value; // Sint16
		  const double ratio = (value - std::numeric_limits<Sint16>::min()) /
		    double(std::numeric_limits<Sint16>::max() - std::numeric_limits<Sint16>::min());

		  const double freq = interpolateFrequency(ratio);
		  const size_t f = std::lround(audioData.freq / freq); // this is the period in samples

		  SDL_LockAudioDevice(audioData.dev);
		  audioData.nf = f;
		  SDL_UnlockAudioDevice(audioData.dev);
		  break;
		}
	      };
	    }
	    break;
	  }
	case SDL_QUIT:
	  {
	    cont = false;

	    // set new volume target to 0
	    // to avoid crackling
	    SDL_LockAudioDevice(audioData.dev);
	    audioData.nv = 0;
	    SDL_UnlockAudioDevice(audioData.dev);
	    break;
	  }
	}
      }
    }

    SDL_JoystickClose(joy);
  }

  // wait a sec so the actual volume drops
  // to reduce crackling on exit
  SDL_Delay(500);

  if (audioData.dev != 0)
  {
    SDL_CloseAudioDevice(audioData.dev);
  }

  SDL_Quit();

  return 0;
}
