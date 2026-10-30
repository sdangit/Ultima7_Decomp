#include "u7port.h"
#include "plat.h"
#include "opl3music.h"

namespace NativeOpl3 {

constexpr int TimbreSize = 48;
constexpr int Channels = 32;
constexpr int RhythmVoices = 5;
enum {
	Mod = 0, Car = 5, Feedback = 10, Rhythm = 11, CarTouch = 12, ModTouch = 13,
	BendRange = 14, Wheel = 15, VibRate = 16, VibDepth = 17, Slide = 18,
	Tune = 36, Layer = 38, KeyShift = 39, ProgramNumber = 47,
};

struct ChannelState {
	int16_t timbre;
	int16_t bend, vibratoDepth, volume;
	uint8_t vibratoRate, pan;
	bool sustain;
};

struct Voice {
	int16_t timbre, pitch, slide, vibrato;
	uint8_t channel, note, state, phase, pan;
	bool keyDown, sustained;
	uint32_t age, noteEvent;
};

struct MusicState {
	uint8_t *bank;
	int count, limit;
	uint32_t clock;
	uint8_t rhythmBits;
	uint8_t feedback[MusicVoices];
	ChannelState channels[Channels];
	Voice voices[MusicVoices];
	Voice drums[RhythmVoices];
};

static MusicState Music;
static uint16_t Port = 0x388;
static const uint8_t DrumChannel[RhythmVoices] = { 6, 7, 8, 8, 7 };
static const uint8_t DrumOperator[RhythmVoices] = { 16, 20, 18, 21, 17 };
static const uint8_t DrumBit[RhythmVoices] = { 16, 8, 4, 2, 1 };
static const int16_t FNumber[13] = { 485, 514, 544, 577, 611, 647, 686, 727, 770, 816, 864, 915, 970 };
static const uint8_t SlideStage[5] = { 24, 0, 18, 20, 22 };

static void Write(uint16_t reg, uint8_t value)
{
	uint16_t port = Port + ((reg & 0x100) ? 2 : 0);
	plat_opl_write(port, reg & 0xff);
	plat_opl_write(port + 1, value);
}

static uint8_t Byte(int timbre, int at)
{
	if (timbre < 0 || timbre >= Music.count || at < 0 || at >= TimbreSize)
		return 0;
	return Music.bank[timbre * TimbreSize + at];
}

static int16_t Word(int timbre, int at)
{
	return (int16_t)(Byte(timbre, at) | Byte(timbre, at + 1) << 8);
}

static int16_t Sar(int16_t value, uint8_t count)
{
	count &= 31;
	return count > 15 ? (value < 0 ? -1 : 0) : value >> count;
}

static bool Available(int voice)
{
	return voice < Music.limit && !(Music.rhythmBits && voice >= 6 && voice <= 8);
}

static uint16_t Frequency(int16_t pitch)
{
	int semitone = pitch >> 8;
	int block = (semitone + 6) / 12 - 2;
	int index = ((semitone + 6) % 12 + 12) % 12;
	int16_t f = FNumber[index];
	if (pitch & 0xff)
		f += (int16_t)((FNumber[index + 1] - f) * (pitch & 0xff)) >> 8;
	return (min(7, max(0, block)) << 10) + f;
}

static uint16_t VoiceFrequency(const Voice &voice)
{
	int16_t pitch = voice.pitch + Music.channels[voice.channel].bend + voice.slide
		+ voice.vibrato + Word(voice.timbre, Tune);
	return Frequency(pitch);
}

static void Retune(Voice &voice, int physical, bool held)
{
	uint16_t freq = VoiceFrequency(voice) + (held ? 0x2000 : 0);
	uint16_t reg = ChannelRegister(physical);
	Write(0xa0 + reg, freq & 0xff);
	Write(0xb0 + reg, freq >> 8);
}

static void ClearVoice(Voice &voice)
{
	memset(&voice, 0, sizeof voice);
	voice.timbre = -1;
	voice.channel = 0xff;
	voice.state = 1;
}

int16_t Init(void *timbres, int voices)
{
	memset(&Music, 0, sizeof Music);
	if (!timbres)
		return 0;
	Music.count = *(uint8_t *)timbres;
	Music.bank = (uint8_t *)timbres + 1;
	Music.limit = min(MusicVoices, max(0, voices));
	for (ChannelState &channel : Music.channels) {
		channel.timbre = -1;
		channel.volume = 0x100;
		channel.pan = 64;
	}
	for (Voice &voice : Music.voices)
		ClearVoice(voice);
	for (Voice &voice : Music.drums)
		ClearVoice(voice);
	Write(0x105, 3);
	Write(0x104, 0);
	Write(0x01, 0x20);
	Write(0xbd, 0);
	Write(0x08, 0);
	for (int v = 0; v < Music.limit; v++) {
		Write(0xa0 + ChannelRegister(v), 0);
		Write(0xb0 + ChannelRegister(v), 0);
		Write(0xc0 + ChannelRegister(v), 0x30);
		Write(0xd0 + ChannelRegister(v), plat_opl_pan_value(64));
	}
	return 1;
}

void SetPort(int16_t port)
{
	Port = (uint16_t)port;
}

static void Release(Voice &voice, int physical, int drum = -1)
{
	voice.keyDown = voice.sustained = false;
	voice.state = 0;
	voice.age = ++Music.clock;
	if (drum >= 0) {
		Music.rhythmBits &= ~DrumBit[drum];
		Write(0xbd, Music.rhythmBits);
	} else
		Retune(voice, physical, false);
}

static void StopChannel(int channel)
{
	for (int v = 0; v < Music.limit; v++)
		if (Available(v) && Music.voices[v].channel == channel) {
			Release(Music.voices[v], v);
			ClearVoice(Music.voices[v]);
		}
	for (int d = 0; d < RhythmVoices; d++)
		if (Music.drums[d].channel == channel) {
			Release(Music.drums[d], DrumChannel[d], d);
			ClearVoice(Music.drums[d]);
		}
}

void Shutdown(void)
{
	Write(0xbd, 0);
	for (int v = 0; v < Music.limit; v++) {
		Write(0xa0 + ChannelRegister(v), 0);
		Write(0xb0 + ChannelRegister(v), 0);
		ClearVoice(Music.voices[v]);
	}
	for (Voice &voice : Music.drums)
		ClearVoice(voice);
	Music.rhythmBits = 0;
	Music.limit = 0;
}

static void EnableRhythm(void)
{
	if (Music.rhythmBits)
		return;
	for (int v = 6; v <= 8; v++) {
		Write(0xa0 + v, 0);
		Write(0xb0 + v, 0);
		ClearVoice(Music.voices[v]);
	}
	Write(0xb7, 0x0a);
	Write(0xa8, 0x54);
	Write(0xb8, 0x09);
	Music.rhythmBits = 0x20;
	Write(0xbd, Music.rhythmBits);
}

static void FinishRhythm(void)
{
	if (!Music.rhythmBits)
		return;
	for (const Voice &drum : Music.drums)
		if (drum.keyDown || drum.sustained)
			return;
	Music.rhythmBits = 0;
	Write(0xbd, 0);
	for (int v = 6; v <= 8; v++) {
		Write(0xa0 + v, 0);
		Write(0xb0 + v, 0);
		ClearVoice(Music.voices[v]);
	}
	for (Voice &drum : Music.drums)
		ClearVoice(drum);
}

static void LoadTimbre(int physical, int timbre, int drum)
{
	uint16_t modulator = drum >= 0 ? DrumOperator[drum] : ModulatorRegister(physical);
	uint16_t carrier = ModulatorRegister(physical) + 3;
	static const uint8_t regs[5] = { 0x20, 0x40, 0x60, 0x80, 0xe0 };
	for (int r = 0; r < 5; r++)
		Write(modulator + regs[r], Byte(timbre, Mod + r) & (r == 4 ? 3 : 0xff));
	if (drum <= 0)
		for (int r = 0; r < 5; r++)
			Write(carrier + regs[r], Byte(timbre, Car + r) & (r == 4 ? 3 : 0xff));
	if (drum <= 0)
		Music.feedback[physical] = Byte(timbre, Feedback) & 0x0f;
	Write(0xc0 + ChannelRegister(physical), Music.feedback[physical] | 0x30);
}

static int Allocate(bool primary)
{
	int oldestFree = -1, oldestBusy = -1;
	for (int v = 0; v < Music.limit; v++) {
		if (!Available(v))
			continue;
		const Voice &voice = Music.voices[v];
		if (voice.state <= 1) {
			if (oldestFree < 0 || voice.age < Music.voices[oldestFree].age)
				oldestFree = v;
		} else if (oldestBusy < 0 || voice.age < Music.voices[oldestBusy].age)
			oldestBusy = v;
	}
	if (oldestFree >= 0)
		return oldestFree;
	if (!primary || oldestBusy < 0)
		return -1;
	Write(0xa0 + ChannelRegister(oldestBusy), 0);
	Write(0xb0 + ChannelRegister(oldestBusy), 0);
	ClearVoice(Music.voices[oldestBusy]);
	return oldestBusy;
}

static void Levels(int physical, int drum, int timbre, int channel, int velocity)
{
	uint16_t carrier = drum > 0 ? DrumOperator[drum] : ModulatorRegister(physical) + 3;
	uint16_t modulator = drum >= 0 ? DrumOperator[drum] : ModulatorRegister(physical);
	int16_t volume = Music.channels[channel].volume;
	uint8_t level = Byte(timbre, Car + 1);
	if (Byte(timbre, CarTouch) || volume < 0x100) {
		int x = Sar(0x3f - velocity, 7 - Byte(timbre, CarTouch)) + (level & 0x3f);
		x = 0x3f - Sar((int16_t)(volume * (0x3f - x)), 8);
		Write(carrier + 0x40, (level & 0xc0) + min(0x3f, max(0, x)));
	}
	level = Byte(timbre, Mod + 1);
	if (Byte(timbre, ModTouch) || volume < 0x100) {
		int x = Sar(0x3f - velocity, 7 - Byte(timbre, ModTouch)) + (level & 0x3f);
		int scaled = x;
		if (Byte(timbre, Feedback) & 1) {
			scaled = Sar((int16_t)(volume * (0x3f - x)), 8);
			x = 0x3f - scaled;
		}
		if (scaled > 0x3f)
			x = 0x3f;
		Write(modulator + 0x40, (level & 0xc0) + max(0, x));
	}
}

void Note(int16_t channel, int16_t note, int16_t velocity)
{
	if (channel < 1 || channel > Channels || !Music.limit)
		return;
	int ch = channel - 1;
	if (!velocity) {
		uint32_t event = UINT32_MAX;
		for (const Voice &voice : Music.voices)
			if (voice.keyDown && voice.channel == ch && voice.note == note)
				event = min(event, voice.noteEvent);
		for (const Voice &voice : Music.drums)
			if (voice.keyDown && voice.channel == ch && voice.note == note)
				event = min(event, voice.noteEvent);
		/* Release one note and all its layers when pitches overlap. */
		for (int v = 0; v < Music.limit; v++) {
			Voice &voice = Music.voices[v];
			if (!Available(v) || !voice.keyDown || voice.channel != ch || voice.note != note
				|| voice.noteEvent != event)
				continue;
			voice.keyDown = false;
			if (Music.channels[ch].sustain)
				voice.sustained = true;
			else
				Release(voice, v);
		}
		for (int d = 0; d < RhythmVoices; d++) {
			Voice &voice = Music.drums[d];
			if (!voice.keyDown || voice.channel != ch || voice.note != note || voice.noteEvent != event)
				continue;
			voice.keyDown = false;
			if (Music.channels[ch].sustain)
				voice.sustained = true;
			else
				Release(voice, DrumChannel[d], d);
		}
		return;
	}
	uint32_t event = ++Music.clock;
	int first = Music.channels[ch].timbre;
	for (int t = first; t >= 0 && t < Music.count; t++) {
		int drum = (int)Byte(t, Rhythm) - 6;
		int physical;
		Voice *voice;
		if (drum >= 0 && drum < RhythmVoices) {
			EnableRhythm();
			physical = DrumChannel[drum];
			Music.rhythmBits &= ~DrumBit[drum];
			Write(0xbd, Music.rhythmBits);
			voice = &Music.drums[drum];
		} else {
			drum = -1;
			physical = Allocate(t == first);
			if (physical < 0) {
				if (!Byte(t, Layer))
					break;
				continue;
			}
			voice = &Music.voices[physical];
		}
		if (voice->channel != ch || voice->timbre != t || drum >= 0)
			LoadTimbre(physical, t, drum);
		voice->pan = Music.channels[ch].pan;
		/* Paired drums share the most recent trigger's pan. */
		Write(0xd0 + ChannelRegister(physical), plat_opl_pan_value(voice->pan));
		voice->channel = ch;
		voice->timbre = t;
		voice->note = note;
		voice->state = 2;
		voice->keyDown = true;
		voice->sustained = false;
		voice->age = ++Music.clock;
		voice->noteEvent = event;
		voice->vibrato = 0;
		voice->phase = 0;
		voice->slide = Word(t, Slide);
		int16_t pitch = (uint16_t)(uint8_t)(note - 60) << 8;
		uint8_t shift = Byte(t, KeyShift);
		pitch = (int8_t)shift < 0 ? Sar(-pitch, ~shift) : Sar(pitch, shift);
		voice->pitch = pitch + (60 << 8);
		Levels(physical, drum, t, ch, velocity);
		if (drum <= 0)
			Retune(*voice, physical, drum < 0);
		if (drum >= 0) {
			Music.rhythmBits |= DrumBit[drum];
			Write(0xbd, Music.rhythmBits);
		}
		if (!Byte(t, Layer))
			break;
	}
}

void Pitch(int16_t channel, int16_t low, int16_t high)
{
	if (channel < 1 || channel > Channels)
		return;
	int ch = channel - 1;
	int32_t bend = (int32_t)(int16_t)((high << 7) + low - 0x2000)
		* Byte(Music.channels[ch].timbre, BendRange);
	Music.channels[ch].bend = (int16_t)(bend >> 8);
	for (int v = 0; v < Music.limit; v++)
		if (Available(v) && Music.voices[v].state > 1 && Music.voices[v].channel == ch)
			Retune(Music.voices[v], v, true);
}

static void Sustain(int channel, bool sustain)
{
	Music.channels[channel].sustain = sustain;
	if (sustain)
		return;
	for (int v = 0; v < Music.limit; v++)
		if (Available(v) && Music.voices[v].channel == channel && Music.voices[v].sustained)
			Release(Music.voices[v], v);
	for (int d = 0; d < RhythmVoices; d++)
		if (Music.drums[d].channel == channel && Music.drums[d].sustained)
			Release(Music.drums[d], DrumChannel[d], d);
}

static void Pan(int channel, int pan)
{
	Music.channels[channel].pan = min(127, max(0, pan));
}

void Control(int16_t channel, int16_t control, int16_t value)
{
	if (channel < 1 || channel > Channels)
		return;
	if (channel == 10)
		for (int ch = 11; ch <= 30; ch++)
			Control(ch, control, value);
	int ch = channel - 1;
	ChannelState &state = Music.channels[ch];
	switch (control) {
	case 1:
		state.vibratoDepth = Sar((int16_t)(Byte(state.timbre, Wheel) * value), 7)
			+ Byte(state.timbre, VibDepth);
		break;
	case 7:
		state.volume = value + 0x80;
		break;
	case 10:
		Pan(ch, value);
		break;
	case 64:
		Sustain(ch, value >= 64);
		break;
	case 0x79:
		Sustain(ch, false);
		Control(channel, 1, 0);
		Control(channel, 7, 0x7f);
		Pan(ch, 64);
		Pitch(channel, 0, 0x40);
		break;
	case 0x78:
	case 0x7b:
		StopChannel(ch);
		FinishRhythm();
		break;
	}
}

void Program(int16_t channel, int16_t program)
{
	if (channel < 1 || channel > Channels || !Music.count)
		return;
	int ch = channel - 1, t;
	for (t = 0; t < Music.count; t++)
		if (Byte(t, ProgramNumber) == (uint8_t)program)
			break;
	if (t == Music.count)
		t = 0;
	StopChannel(ch);
	Music.channels[ch].timbre = t;
	Music.channels[ch].vibratoRate = Byte(t, VibRate);
	Music.channels[ch].vibratoDepth = Byte(t, VibDepth);
	if (Byte(t, Rhythm))
		EnableRhythm();
}

void Timer(void)
{
	for (int v = 0; v < Music.limit; v++) {
		Voice &voice = Music.voices[v];
		if (!Available(v) || voice.channel >= Channels || voice.timbre < 0)
			continue;
		bool changed = false;
		int stage = voice.state < 5 ? SlideStage[voice.state] : 0;
		if (stage) {
			int16_t rate = Word(voice.timbre, stage * 2 - 16);
			int16_t target = Word(voice.timbre, stage * 2 - 14);
			int distance = target > voice.slide ? target - voice.slide : voice.slide - target;
			if ((uint16_t)distance < (uint16_t)rate) {
				voice.slide = target;
				voice.state++;
			} else
				voice.slide += voice.slide < target ? rate : -rate;
			changed = true;
		}
		const ChannelState &channel = Music.channels[voice.channel];
		if (channel.vibratoRate) {
			voice.phase += channel.vibratoRate;
			int8_t phase = (int8_t)voice.phase;
			if (phase > 0x3f || phase < -0x40)
				phase = (int8_t)(0x80 - phase);
			voice.vibrato = Sar((int16_t)(channel.vibratoDepth * phase), 4);
			changed = true;
		}
		if (changed || Word(voice.timbre, Tune))
			Retune(voice, v, voice.state > 1);
	}
}

}

extern "C" void ResetOpl3musicGlobals(void)
{
	memset(&NativeOpl3::Music, 0, sizeof NativeOpl3::Music);
	NativeOpl3::Port = 0x388;
}
