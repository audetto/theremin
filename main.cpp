#include <SDL.h>
#include <iostream>
#include <cmath>
#include <limits>
#include <list>

namespace
{

  double sawtooth(double x)
  {
    // period 1, range [-1, 1]
    return 2.0 * (x - std::floor(0.5 + x));
  }

  double triangle(double x)
  {
    // period 1, range [-1, 1]
    return 2.0 * std::abs(sawtooth(x)) - 1.0;
  }

  double square(double x)
  {
    return x - std::floor(x) > 0.5 ? 1.0 : 0.0;
  }

  double wave(double x, int type)
  {
    switch (type % 4)
    {
    case 0: return std::sin(2.0 * M_PI * x);
    case 1: return sawtooth(x);
    case 2: return triangle(x);
    case 3: return square(x);
    default: return 0;
    }
  }

  struct Note
  {
    Note (double f, double v, double s, double a, double t)
      : frequency(f), volume(v), start(s), amplitude(a), target(t)
    {
    }

    const double frequency;
    const double volume;

    const double start;

    double amplitude;
    double target;
  };

  struct AudioData
  {
    SDL_AudioDeviceID dev;

    double t; // time from the begin
    double dt; // sample period (e.g. 1 / 48000)

    double decay;
    double threshold;

    int type;

    std::list<Note> notes;
  };

  void audioCallback(void* userdata, Uint8* stream, int len)
  {
    AudioData * audioData = static_cast<AudioData *>(userdata);

    Sint16* buf = (Sint16*)stream;
    const int bytesPerSample = sizeof(Sint16) / sizeof(Uint8);

    const double coeff = std::exp(- audioData->dt * audioData->decay);

    const size_t numberOfSamples = len / bytesPerSample;

    for (size_t i = 0; i < numberOfSamples; ++i)
    {
      std::list<Note>::iterator it = audioData->notes.begin();

      double w = 0.0;

      while (it != audioData->notes.end())
      {
	Note & n = *it;

	const double x = n.frequency * (audioData->t - n.start);
	const double value = n.amplitude * n.volume * wave(x, audioData->type);
	w += value;

	// step towards the target
	n.amplitude = n.target + (n.amplitude - n.target) * coeff;

	if (std::abs(n.amplitude - n.target) < audioData->threshold)
	{
	  n.amplitude = n.target;
	}

	if (n.amplitude == 0.0)
	{
	  it = audioData->notes.erase(it);
	}
	else
	{
	  ++it;
	}
      }

      w = std::min(w, double(std::numeric_limits<Sint16>::max()));
      w = std::max(w, double(std::numeric_limits<Sint16>::min()));
      buf[i] = w;
      audioData->t += audioData->dt;
    }
  }

  void addNote(AudioData & audioData, const double frequency, const double volume)
  {
    std::cout << "NEW NOTE: " << frequency << " @ " << volume << " [" << audioData.notes.size() << "]" << std::endl;

    Note & top = audioData.notes.front();

    // current note will go to silent
    top.target = 0.0;

    double start = audioData.t;
    if (frequency != 0.0)
    {
      const double currentFrequency = top.frequency;
      const double currentStart = top.start;

      // give the new note the same phase as the old one
      start -= (audioData.t - currentStart) * currentFrequency / frequency;
    }

    const double amplitude = 0.0;  // start silent
    const double target = 1.0;     // and grow to full size

    // only lock for the minimum time required
    SDL_LockAudioDevice(audioData.dev);
    audioData.notes.emplace_front(frequency, volume, start, amplitude, target);
    SDL_UnlockAudioDevice(audioData.dev);
  }

  void openAudio(AudioData & audioData)
  {
    SDL_AudioSpec want, have;

    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.channels = 1;
    want.callback = &audioCallback;
    want.userdata = &audioData;

    audioData.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

    if (audioData.dev == 0)
    {
      throw std::runtime_error("Failed to open audio: ");
    }
    else
    {
      audioData.dt = 1.0 / have.freq;
      audioData.t = 0.0;

      audioData.decay = 50.0;
      audioData.threshold = 0.0000001;

      audioData.type = 0;

      // add silence
      audioData.notes.emplace_front(0.0, 0.0, 0.0, 1.0, 1.0);

      SDL_PauseAudioDevice(audioData.dev, 0);
    }

  }

  // x in [0, 1]
  // output in [440 / 2, 440 * 2]
  // 2 octaves
  double interpolateFrequency(double x)
  {
    const double halfRange = 2.0;
    const double k = std::log(halfRange);
    const double mult = std::exp(2.0 * k * x) / halfRange;

    const double freq = 440.0 * mult;
    return freq;
  }

  void theremin()
  {
    const int ok = SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_AUDIO);

    if (ok)
    {
      throw std::runtime_error("Failed to initialise SDL");
    }

    AudioData audioData;

    openAudio(audioData);

    const int joyID = 0;
    SDL_Joystick* joy = SDL_JoystickOpen(joyID);

    if (!joy)
    {
      throw std::runtime_error("Failed to open joystick");
    }

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
		  const int value = -je.value; // this is a Sint16

		  // this number is between 0 and 32767.5 = 32767
		  const int volume = (value - std::numeric_limits<Sint16>::min()) / 2;

		  addNote(audioData, audioData.notes.front().frequency, volume);
		  break;
		}
	      case 4: // freq
		{
		  const int value = -je.value; // Sint16
		  const double ratio = (value - std::numeric_limits<Sint16>::min()) /
		    double(std::numeric_limits<Sint16>::max() - std::numeric_limits<Sint16>::min());

		  const double frequency = interpolateFrequency(ratio);

		  addNote(audioData, frequency, audioData.notes.front().volume);
		  break;
		}
	      };
	    }
	    break;
	  }
	case SDL_JOYBUTTONDOWN:
	  {
	    const SDL_JoyButtonEvent & je = event.jbutton;

	    if (id == je.which)
	    {
	      if (je.button == 8) // big middle button
	      {
		// ask SDL to quit
		SDL_Event quit;
		quit.type = SDL_QUIT;
		quit.quit.timestamp = event.jbutton.timestamp;
		SDL_PushEvent(&quit);
	      }
	      else
	      {
		// add some silence
		addNote(audioData, audioData.notes.front().frequency, 0.0);

		// and let it happen to get smooth change
		SDL_Delay(100);

		SDL_LockAudioDevice(audioData.dev);
		++audioData.type; // loop wave type
		SDL_UnlockAudioDevice(audioData.dev);
	      }
	    }
	    break;
	  }
	case SDL_QUIT:
	  {
	    cont = false;

	    // set new volume target to 0
	    // to avoid crackling
	    addNote(audioData, audioData.notes.front().frequency, 0.0);
	    break;
	  }
	}
      }
    }

    SDL_JoystickClose(joy);

    // wait a sec so the actual volume drops
    // to reduce crackling on exit
    SDL_Delay(500);

    SDL_CloseAudioDevice(audioData.dev);

    SDL_Quit();
  }

}

int main(int /* argc */,  char** /* argv */)
{
  try
  {
    theremin();
    return 0;
  }
  catch (const std::exception & e)
  {
    std::cout << e.what() << std::endl;
    return 1;
  }
}
